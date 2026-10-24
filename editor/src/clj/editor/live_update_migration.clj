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

(ns editor.live-update-migration
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.collection-proxy :as collection-proxy]
            [editor.defold-project :as project]
            [editor.disk :as disk]
            [editor.form :as form]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.bob.archive.liveupdate PackageOutputPath PackageResolver]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.io File]
           [java.nio.file FileAlreadyExistsException Files LinkOption OpenOption Path StandardOpenOption]
           [java.util.function Function]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- proxy-collection-paths [evaluation-context]
  (let [basis (:basis evaluation-context)]
    (reduce (fn [paths node-id]
              (if-not (g/node-instance? basis collection-proxy/CollectionProxyNode node-id)
                paths
                (let [collection (g/node-value node-id :collection evaluation-context)]
                  (if-not (resource/resource? collection)
                    paths
                    (update paths
                            (if (g/node-value node-id :exclude evaluation-context) :excluded :included)
                            conj (resource/proj-path collection))))))
            {:excluded (sorted-set) :included (sorted-set)}
            (g/node-ids basis))))

(defn- migration-manifest [^File project-directory {:keys [excluded included]} settings]
  (let [provider (if (= "Folder" (get settings ["liveupdate" "mode"])) "folder" "zip")
        output-name (get settings ["liveupdate" (if (= "folder" provider) "output-folder-name" "zip-filename")] "")
        package-name (if-not (and (= "zip" provider) (string/ends-with? output-name ".zip"))
                       output-name
                       (subs output-name 0 (- (count output-name) 4)))
        output-directory (get settings ["liveupdate" (if (= "folder" provider) "output-directory" "zip-filepath")])
        output-directory (if (string/blank? output-directory) "." output-directory)
        ^Path output-root (try
                            (PackageOutputPath/resolve output-directory (.toPath project-directory)
                                                       (reify Function (apply [_ name] (System/getenv name))))
                            ;; Leave unresolved paths for manifest validation.
                            (catch IllegalArgumentException _ nil))
        compression-level (get settings ["liveupdate" "zip-compression-level"])
        manifest (protobuf/pb->map-without-defaults (PackageResolver/migrationManifest package-name provider excluded included))
        base-name (get-in manifest [:packages 0 :name])
        package-name (loop [suffix 0]
                       (let [candidate (str base-name (when (pos? suffix) suffix))]
                         (if (and output-root
                                  (Files/exists (.resolve output-root (str candidate (when (= "zip" provider) ".zip")))
                                                (into-array LinkOption [LinkOption/NOFOLLOW_LINKS])))
                           (recur (inc suffix))
                           candidate)))]
    (cond-> (-> manifest
               (assoc :output-directory output-directory)
               (assoc-in [:packages 0 :name] package-name))
      (and (= "zip" provider) (some? compression-level))
      (assoc :providers [{:provider "zip" :properties [{:name "compression-level" :value (str compression-level)}]}]))))

(defn- create-manifest! [workspace resource-map content]
  (loop [suffix 0]
    (let [proj-path (str "/liveupdate" (when (pos? suffix) suffix) ".lumanifest")
          manifest-resource (workspace/file-resource workspace proj-path)
          created (when-not (contains? resource-map proj-path)
                    (try
                      ;; CREATE_NEW also protects ignored files and symlinks
                      ;; absent from the workspace resource map.
                      (Files/writeString (.toPath (io/file manifest-resource)) ^String content
                                         (into-array OpenOption [StandardOpenOption/CREATE_NEW StandardOpenOption/WRITE]))
                      manifest-resource
                      (catch FileAlreadyExistsException _ nil)))]
      (if created
        created
        (recur (inc suffix))))))

(defn migrate!
  "Migrate checked collection proxies once, after loading the project and before
  starting its UI. Save only the new manifest and its game.project setting."
  [project]
  (g/let-ec [workspace (project/workspace project evaluation-context)
             localization (workspace/localization workspace evaluation-context)
             game-project (project/get-resource-node project "/game.project" evaluation-context)
             game-project-resource (g/node-value game-project :resource evaluation-context)
             settings (g/node-value game-project :settings-map evaluation-context)
             manifest (when (and (not (g/error? settings))
                                 (not (get settings ["liveupdate" "manifest"]))
                                 (not (resource/read-only? game-project-resource)))
                        (let [collection-paths (proxy-collection-paths evaluation-context)]
                          (when (coll/not-empty (:excluded collection-paths))
                            (let [settings-resource (get settings ["liveupdate" "settings"])
                                  legacy-settings (when (and settings-resource (resource/exists? settings-resource))
                                                    (g/node-value (project/get-resource-node project settings-resource evaluation-context)
                                                                  :settings-map evaluation-context))]
                              (migration-manifest (workspace/project-directory (:basis evaluation-context) workspace)
                                                  collection-paths legacy-settings)))))
             resource-map (when manifest (g/node-value workspace :resource-map evaluation-context))]
    (when manifest
      (let [manifest-resource (create-manifest! workspace resource-map (protobuf/map->str LiveUpdateManifest$ManifestDesc manifest))]
        (workspace/resource-sync! workspace)
        (g/let-ec [tx-data (form/set-value (:form-ops (g/node-value game-project :form-data evaluation-context))
                                          ["liveupdate" "manifest"] manifest-resource)]
          (g/transact {:undoable false} tx-data))
        (disk/process-post-save-actions!
          workspace
          (disk/write-save-data-to-disk! [(g/node-value game-project :save-data)] nil localization nil))
        (notifications/show! (workspace/notifications workspace)
                             {:type :info
                              :id ::migration
                              :message (localization/message "liveupdate.migrated" {"manifest" (resource/proj-path manifest-resource)})})
        manifest-resource))))
