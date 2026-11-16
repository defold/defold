;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.live-update-settings
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [internal.java :as java]
            [util.coll :as coll])
  (:import [com.dynamo.bob CompileExceptionError Progress]
           [com.dynamo.bob.archive.liveupdate PackageArtifacts PackageBuilder PackageResolver]
           [com.dynamo.bob.archive.publisher PublisherSettings PublisherSettings$PublishMode]
           [com.dynamo.bob.fs DefaultFileSystem]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.io StringReader]
           [java.nio.file FileAlreadyExistsException Files OpenOption Path StandardOpenOption]))

(set! *warn-on-reflection* true)

(def live-update-icon "icons/32/Icons_04-Project-file.png")

(g/defnode LiveUpdateSettingsNode
  (inherits resource-node/ResourceNode)

  (input form-data g/Any)
  (output form-data g/Any :cached
          (g/fnk [form-data]
            (assoc form-data :navigation false)))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))
  (output build-targets g/Any :cached (g/constantly [])))

(def ^:private basic-meta-info
  (with-open [rdr (io/reader (io/resource "liveupdate-meta.properties"))]
    (settings-core/load-meta-properties rdr)))

(defn- load-live-update-settings [load-opts {:keys [owner-resource source-value] self :node-id}]
  (g/make-nodes [settings-node settings/SettingsNode]
    (g/connect settings-node :_node-id self :nodes)
    (g/connect settings-node :save-value self :save-value)
    (g/connect settings-node :form-data self :form-data)
    (settings/load-settings-node load-opts self settings-node owner-resource source-value basic-meta-info nil)))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "settings"
    :label (localization/message "resource.type.settings")
    :node-type LiveUpdateSettingsNode
    :load-fn load-live-update-settings
    :meta-settings (:settings basic-meta-info)
    :icon live-update-icon
    :view-types [:form :text]))

(defn get-live-update-settings-path [project]
  (resource/resource->proj-path (get (project/settings project) ["liveupdate" "settings"])))

(defn- legacy-rules [^Path project-directory resource-map save-data]
  ;; Build from a temporary snapshot so unsaved edits participate in conversion.
  (let [generated-paths (PackageArtifacts/generatedPaths project-directory)
        snapshot (fs/create-temp-directory! "defold-liveupdate-conversion-")]
    (try
      (let [contents (coll/into-> save-data {}
                       (keep (fn [[path save-value resource-type]]
                               (when-let [content (resource-node/save-content save-value resource-type)]
                                 [path content]))))]
        (doseq [[path source] resource-map
                :when (and (= :file (resource/source-type source))
                           (or (resource/file-resource? source) (resource/zip-resource? source))
                           (not (string/starts-with? path "/builtins/"))
                           (not (PackageArtifacts/isGeneratedPath path generated-paths)))]
          (let [destination (io/file snapshot (subs path 1))]
            (fs/create-parent-directories! destination)
            (if-let [content (get contents path)]
              (spit destination content :encoding "UTF-8")
              (with-open [input (io/input-stream source)]
                (io/copy input destination)))))
        (let [plan (PackageBuilder/buildPlan (.toPath snapshot) "" {} java/class-loader (Progress/discarding))]
          (mapv protobuf/pb->map-without-defaults (PackageResolver/migrateLegacy plan))))
      (finally
        (fs/delete-directory! snapshot)))))

(defn- package-name-for-publisher [filename zip]
  (let [name (string/replace (or filename "") #"[^a-zA-Z0-9._-]+" "_")
        name (cond-> name zip (string/replace #"\.zip$" ""))]
    (cond
      (string/blank? name) "liveupdate"
      (or (not (re-find #"^[a-zA-Z0-9]" name))
          (re-matches #"(?i)(base|(con|prn|aux|nul|com[1-9]|lpt[1-9])([.].*)?)" name))
      (str "liveupdate_" name)
      :else name)))

(defn- write-manifest! [^Path project-directory resource-map ^String content]
  (let [options (into-array OpenOption [StandardOpenOption/CREATE_NEW StandardOpenOption/WRITE])]
    (loop [index 0]
      (let [name (str "liveupdate" (when (pos? index) index) ".archivemanifest")
            proj-path (str "/" name)]
        (if (or (contains? resource-map proj-path)
                (try
                  (Files/writeString (.resolve project-directory name) content options)
                  false
                  (catch FileAlreadyExistsException _ true)))
          (recur (inc index))
          proj-path)))))

(defn convert-to-manifest!
  "Creates a manifest from the current legacy settings.

  Performs disk I/O and returns the new manifest's project path."
  [project settings-node]
  (g/let-ec [workspace (project/workspace project evaluation-context)
             directory (.toPath (workspace/project-directory (:basis evaluation-context) workspace))
             resource-map (g/node-value workspace :resource-map evaluation-context)
             source (resource/proj-path (g/node-value settings-node :resource evaluation-context))
             text (coll/join-to-string "\n" (g/node-value settings-node :lines evaluation-context))
             compression (if (get (project/settings project evaluation-context) ["project" "compress_archive"])
                           :lz4
                           :none)
             save-data (mapv (fn [{:keys [resource save-value]}]
                               [(resource/proj-path resource)
                                save-value
                                (resource/resource-type resource (:basis evaluation-context))])
                             (project/dirty-save-data project evaluation-context))]
    (let [{:keys [zip zip-level output filename]}
          (try
            (let [publisher (PublisherSettings/parse (StringReader. text))
                  zip (= PublisherSettings$PublishMode/Zip (.getMode publisher))]
              {:zip zip
               :zip-level (when zip (.getCompressionLevel publisher))
               :output (if zip (.getZipFilepath publisher) (.getOutputDirectory publisher))
               :filename (if zip (.getZipFilename publisher) (.getOutputFolderName publisher))})
            (catch Exception error
              (let [file-system (doto (DefaultFileSystem.) (.setRootDirectory (str directory)))]
                (throw (CompileExceptionError. (.get file-system source) -1 (ex-message error) error)))))

          manifest {:output-directory (if (string/blank? output) "." output)
                    :base-file-compression-preset "legacy"
                    :file-compression-presets [{:name "legacy" :file-compression compression}]
                    :container-presets [(cond-> {:name "legacy" :provider (if zip "zip" "folder")}
                                          zip (assoc :zip-level zip-level))]
                    :packages [{:name (package-name-for-publisher filename zip)
                                :container-preset "legacy"
                                :file-compression-preset "legacy"
                                :rules (legacy-rules directory resource-map save-data)}]}
          content (str "# Publisher settings and excluded content converted from "
                       (string/replace source #"[\r\n]" " ") "\n"
                       "# Clear game.project > Live Update > Settings, then select this file in Archive > Manifest.\n"
                       (protobuf/map->str LiveUpdateManifest$ManifestDesc manifest))]
      (write-manifest! directory resource-map content))))
