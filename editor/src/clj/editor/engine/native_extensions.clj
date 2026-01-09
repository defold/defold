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

(ns editor.engine.native-extensions
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.connection-properties :as connection-properties]
            [editor.defold-project :as project]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.system :as system]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.defold.extender.client ExtenderClient ExtenderClientCache ExtenderResource]
           [com.dynamo.bob Platform]
           [java.io File]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.util ArrayList Base64]))

(set! *warn-on-reflection* true)

;;; Caching

(defn- cache-dir ^File
  [project-directory]
  (doto (io/file project-directory ".internal" "cache" "engine-archives")
    (fs/create-directories!)))

;;; Extension discovery/processing

(def ^:private extender-platforms
  {(.getPair Platform/Arm64MacOS)   {:platform      "arm64-osx"
                                     :library-paths #{"osx" "arm64-osx"}}
   (.getPair Platform/X86_64MacOS)  {:platform      "x86_64-osx"
                                     :library-paths #{"osx" "x86_64-osx"}}
   (.getPair Platform/Arm64Ios)     {:platform      "arm64-ios"
                                     :library-paths #{"ios" "arm64-ios"}}
   (.getPair Platform/Armv7Android) {:platform      "armv7-android"
                                     :library-paths #{"android" "armv7-android"}}
   (.getPair Platform/Arm64Android) {:platform      "arm64-android"
                                     :library-paths #{"android" "arm64-android"}}
   (.getPair Platform/JsWeb)        {:platform      "js-web"
                                     :library-paths #{"web" "js-web"}}
   (.getPair Platform/X86Win32)     {:platform      "x86-win32"
                                     :library-paths #{"win32" "x86-win32"}}
   (.getPair Platform/X86_64Win32)  {:platform      "x86_64-win32"
                                     :library-paths #{"win32" "x86_64-win32"}}
   (.getPair Platform/X86_64Linux)  {:platform      "x86_64-linux"
                                     :library-paths #{"linux" "x86_64-linux"}}
   (.getPair Platform/Arm64Linux)   {:platform      "arm64-linux"
                                     :library-paths #{"linux" "arm64-linux"}}})

(def ^:private common-extension-paths
  [["ext.manifest"]
   ["include"]
   ["manifests"]
   ["src"]
   ["commonsrc"]
   ;["pluginsrc"] ; We explicitly don't include this until we can build different build targets from within the editor (e.g. using "build-plugins" as bob does)
   ["lib" "common"]])

(defn- platform-extension-paths
  [platform]
  (into common-extension-paths (map #(vector "lib" %) (get-in extender-platforms [platform :library-paths]))))

(defn- engine-extension-root?
  "Tests if the resource is an extension root that should be built remotely

  Engine extension root should be a folder with ext.manifest file and either src
  or commonsrc folder"
  [resource]
  (= :both
     (reduce
       (fn [acc resource]
         (case (resource/resource-name resource)
           "ext.manifest"
           (if (= acc :source)
             (reduced :both)
             :manifest)

           ("src" "commonsrc")
           (if (= acc :manifest)
             (reduced :both)
             :source)
           acc))
       :none
       (resource/children resource))))

(defn engine-extension-roots
  [project evaluation-context]
  (->> (g/node-value project :resources evaluation-context)
       (filter engine-extension-root?)
       seq))

(defn- resource-child
  [resource name]
  (when resource
    (first (filter #(= name (resource/resource-name %)) (resource/children resource)))))

(defn- resource-by-path
  [resource path]
  (reduce resource-child resource path))

(defn extension-resource-nodes
  [project evaluation-context platform]
  (let [native-extension-roots (engine-extension-roots project evaluation-context)
        paths (platform-extension-paths platform)]
    (->> (for [root native-extension-roots
               path paths
               :let [resource (resource-by-path root path)]
               :when resource]
           resource)
         (mapcat resource/resource-seq)
         (filter #(= :file (resource/source-type %)))
         (map (fn [resource]
                (project/get-resource-node project resource evaluation-context))))))

;;; Extender API

(defn- build-url
  [extender-platform sdk-version]
  (format "/build/%s/%s" extender-platform (or sdk-version "")))

(defn supported-platform? [platform]
  (contains? extender-platforms platform))

;;; Building

(defn get-build-server-url
  (^String [prefs project]
   (g/with-auto-evaluation-context evaluation-context
     (get-build-server-url prefs project evaluation-context)))
  (^String [prefs project evaluation-context]
   (or (not-empty (string/trim (prefs/get prefs [:extensions :build-server]))) ;; always trim because `prefs/get` does not return nil
       (not-empty (some-> (shared-editor-settings/get-setting project ["extensions" "build_server"] evaluation-context) string/trim)) ;; use `some->` because `get-setting` may return nil
       (connection-properties/defold-build-server-url))))

(defn get-build-server-headers
  "Returns a (possibly empty) vector of header strings"
  [prefs]
  (into [] (remove string/blank?) (string/split-lines (prefs/get prefs [:extensions :build-server-headers]))))

;; Note: When we do bundling for Android via the editor, we need add
;;       [["android" "proguard"] "_app/app.pro"] to the returned table.
(defn- global-resource-nodes-by-upload-path [project evaluation-context]
  (let [project-settings (g/node-value project :settings evaluation-context)]
    (into {}
          (keep (fn [[[section key] target]]
                  (when-let [proj-path (get project-settings [section key])]
                    (let [resource-node (project/get-resource-node project proj-path evaluation-context)]
                      (if (some-> resource-node (g/node-value :resource evaluation-context) resource/exists?)
                        [target resource-node]
                        (throw (engine-build-errors/missing-resource-error
                                 "Missing Native Extension Resource"
                                 proj-path
                                 (project/get-resource-node project "/game.project" evaluation-context))))))))
          [[["native_extension" "app_manifest"] "_app/app.manifest"]])))

(defn- get-ne-platform [platform]
  (case platform
    "arm64-macos" "arm64-osx"
    "x86_64-macos" "x86_64-osx"
    platform))

(defn- get-main-manifest-section-and-key [platform]
   (case platform
     "armv7-android"    ["android" "manifest"]
     "arm64-android"    ["android" "manifest"]
     "arm64-ios"        ["ios" "infoplist"]
     "armv7-ios"        ["ios" "infoplist"]
     "arm64-osx"        ["osx" "infoplist"]
     "x86_64-osx"       ["osx" "infoplist"]
     "js-web"           ["html5" "htmlfile"]
     "wasm-web"         ["html5" "htmlfile"]
     "wasm_pthread-web" ["html5" "htmlfile"]))

(defn- get-main-manifest-name [ne-platform]
  (case ne-platform
    "armv7-android"    "AndroidManifest.xml"
    "arm64-android"    "AndroidManifest.xml"
    "arm64-ios"        "Info.plist"
    "armv7-ios"        "Info.plist"
    "arm64-osx"        "Info.plist"
    "x86_64-osx"       "Info.plist"
    "js-web"           "engine_template.html"
    "wasm-web"         "engine_template.html"
    "wasm_pthread-web" "engine_template.html"
    nil))

(defn- get-main-manifest-file-upload-resource [project evaluation-context platform]
  (let [ne-platform (get-ne-platform platform)
        target-path (get-main-manifest-name ne-platform)]
    (when target-path
      (let [project-settings (g/node-value project :settings evaluation-context)
            [section key] (get-main-manifest-section-and-key ne-platform)
            resource (get project-settings [section key])
            resource-node (project/get-resource-node project resource evaluation-context)]
        {target-path resource-node}))))

(defn- resource-node-upload-path [resource-node evaluation-context]
  (fs/without-leading-slash (resource/proj-path (g/node-value resource-node :resource evaluation-context))))

(defn- extension-resource-nodes-by-upload-path [project evaluation-context platform]
  (into {}
        (map (juxt #(resource-node-upload-path % evaluation-context) identity))
        (extension-resource-nodes project evaluation-context platform)))

(defn has-engine-extensions?
  "Returns true if the project's engine should be built remotely"
  [project evaluation-context]
  (boolean
    (or (seq (engine-extension-roots project evaluation-context))
        (pos? (count (global-resource-nodes-by-upload-path project evaluation-context))))))

(defn- make-extender-resources [project platform evaluation-context]
  (reduce-kv
    (fn [^ArrayList acc upload-path resource-node]
      (doto acc
        (.add (reify ExtenderResource
                (getPath [_] upload-path)
                (getContent [_]
                  (if-let [content (some-> (g/node-value resource-node :save-data evaluation-context)
                                           (resource-node/save-data-content))]
                    (.getBytes content StandardCharsets/UTF_8)
                    (with-open [is (io/input-stream (g/node-value resource-node :resource evaluation-context))]
                      (.readAllBytes is))))
                (getLastModified [_]
                  (.lastModified (io/file (g/node-value resource-node :resource evaluation-context))))))))
    (ArrayList.)
    (merge (global-resource-nodes-by-upload-path project evaluation-context)
           (extension-resource-nodes-by-upload-path project evaluation-context platform)
           (get-main-manifest-file-upload-resource project evaluation-context platform))))

(defn get-engine-archive [project platform prefs evaluation-context]
  (if-not (supported-platform? platform)
    (throw (engine-build-errors/unsupported-platform-error platform))
    (let [basis (:basis evaluation-context)
          extender-platform (get-in extender-platforms [platform :platform])
          project-directory (workspace/project-directory basis (project/workspace project evaluation-context))
          cache-directory (cache-dir project-directory)
          sdk-version (system/defold-engine-sha1)
          cache (ExtenderClientCache. cache-directory)
          extender-resources (make-extender-resources project platform evaluation-context)
          cache-key (.calcKey cache extender-platform sdk-version extender-resources)
          url (get-build-server-url prefs project evaluation-context)
          headers (get-build-server-headers prefs)
          username (string/trim (prefs/get prefs [:extensions :build-server-username]))
          password (prefs/get prefs [:extensions :build-server-password])]
      (if (.isCached cache extender-platform cache-key)
        {:id {:type :custom :version cache-key}
         :cached true
         :engine-archive (.getCachedBuildFile cache extender-platform)
         :extender-platform extender-platform}
        (let [extender-client (ExtenderClient. url cache-directory)
              destination-file (fs/create-temp-file! (str "build_" sdk-version) ".zip")
              log-file (fs/create-temp-file! (str "build_" sdk-version) ".txt")]
          (try
            (when-let [^String auth (or
                                      (and (not (string/blank? username))
                                           (not (coll/empty? password))
                                           (str username ":" password))
                                      (.getUserInfo (URI. url)))]
              (.setHeader extender-client "Authorization" (str "Basic " (.encodeToString (Base64/getEncoder) (.getBytes auth StandardCharsets/UTF_8)))))
            (when (pos? (count headers))
              (.setHeaders extender-client headers))
            (.build extender-client extender-platform sdk-version extender-resources destination-file log-file)
            {:id {:type :custom :version cache-key}
             :engine-archive destination-file
             :extender-platform extender-platform}
            (catch Exception e
              (throw (engine-build-errors/build-error
                       (or (:cause (Throwable->map e))
                           (.getSimpleName (class e)))
                       (slurp log-file))))))))))
