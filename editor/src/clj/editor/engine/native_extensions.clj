;; Copyright 2020-2022 The Defold Foundation
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
  (:require
   [clojure.java.io :as io]
   [clojure.data.json :as json]
   [dynamo.graph :as g]
   [editor.connection-properties :refer [connection-properties]]
   [editor.prefs :as prefs]
   [editor.defold-project :as project]
   [editor.engine.build-errors :as engine-build-errors]
   [editor.fs :as fs]
   [editor.resource :as resource]
   [editor.system :as system]
   [editor.workspace :as workspace]
   [util.http-client :as http])
  (:import
   (java.io File)
   (java.net URI)
   (java.util Base64)
   (java.security MessageDigest)
   (javax.ws.rs.core MediaType)
   (org.apache.commons.codec.binary Hex)
   (org.apache.commons.codec.digest DigestUtils)
   (org.apache.commons.io IOUtils)
   (com.dynamo.bob Platform)
   (com.sun.jersey.api.client.config DefaultClientConfig)
   (com.sun.jersey.api.client Client ClientResponse WebResource$Builder)
   (com.sun.jersey.core.impl.provider.entity InputStreamProvider StringProvider)
   (com.sun.jersey.multipart FormDataMultiPart)
   (com.sun.jersey.multipart.impl MultiPartWriter)
   (com.sun.jersey.multipart.file StreamDataBodyPart)))

(set! *warn-on-reflection* true)

(def ^:const defold-build-server-url (get-in connection-properties [:native-extensions :build-server-url]))
(def ^:const connect-timeout-ms (* 30 1000))
(def ^:const read-timeout-ms (* 10 60 1000))

;;; Caching

(defn- hash-resources! ^MessageDigest
  [^MessageDigest md resource-nodes evaluation-context]
  (run! #(DigestUtils/updateDigest md ^String (g/node-value % :sha256 evaluation-context))
        resource-nodes)
  md)

(defn- cache-key
  [^String extender-platform ^String sdk-version resource-nodes evaluation-context]
  (-> (DigestUtils/getSha256Digest)
      (DigestUtils/updateDigest extender-platform)
      (DigestUtils/updateDigest (or sdk-version ""))
      (hash-resources! resource-nodes evaluation-context)
      (.digest)
      (Hex/encodeHexString)))

(defn- cache-file ^File
  [cache-dir extender-platform]
  (doto (io/file cache-dir extender-platform "build.zip")
    (fs/create-parent-directories!)))

(defn- cache-file-hash ^File
  [cache-dir extender-platform]
  (doto (io/file cache-dir extender-platform "build.hash")
    (fs/create-parent-directories!)))

(defn- cache-dir ^File
  [project-directory]
  (doto (io/file project-directory ".internal" "cache" "engine-archives")
    (fs/create-parent-directories!)))

(defn- cached-engine-archive
  [cache-dir extender-platform key]
  (let [cache-file (cache-file cache-dir extender-platform)
        cache-file-hash (cache-file-hash cache-dir extender-platform)]
    (when (and (.exists cache-file)
               (.exists cache-file-hash)
               (= key (slurp cache-file-hash)))
      cache-file)))

(defn- cache-engine-archive!
  [cache-dir extender-platform key ^File engine-archive]
  (let [cache-file (cache-file cache-dir extender-platform)
        cache-file-hash (cache-file-hash cache-dir extender-platform)]
    (fs/move-file! engine-archive cache-file)
    (spit cache-file-hash key)
    cache-file))


;;; Extension discovery/processing

(def ^:private extender-platforms
  {(.getPair Platform/X86_64MacOS)  {:platform      "x86_64-osx"
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
                                     :library-paths #{"linux" "x86_64-linux"}}})

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

(defn- extension-root?
  [resource]
  (some #(= "ext.manifest" (resource/resource-name %)) (resource/children resource)))

(defn extension-roots
  [project evaluation-context]
  (->> (g/node-value project :resources evaluation-context)
       (filter extension-root?)
       (seq)))

(defn- resource-child
  [resource name]
  (when resource
    (first (filter #(= name (resource/resource-name %)) (resource/children resource)))))

(defn- resource-by-path
  [resource path]
  (reduce resource-child resource path))

(defn extension-resource-nodes
  [project evaluation-context platform]
  (let [native-extension-roots (extension-roots project evaluation-context)
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

(defn- resource-node-content-stream ^java.io.InputStream
  [resource-node evaluation-context]
  (if-let [content (some-> (g/node-value resource-node :save-data evaluation-context) :content)]
    (IOUtils/toInputStream ^String content "UTF-8")
    (io/input-stream (g/node-value resource-node :resource evaluation-context))))

(defn supported-platform? [platform]
  (contains? extender-platforms platform))

;;; Building

(defn- make-cache-request
  [server-url payload]
  (let [parts (http/split-url server-url)]
    {:request-method :post
     :scheme         (get parts :protocol)
     :server-name    (get parts :host)
     :server-port    (get parts :port)
     :uri            "/query"
     :content-type   "application/json"
     :headers        {"Accept" "application/json"}
     :body           payload}))

(defn- query-cached-files
  "Asks the server what files it already has.
  This is to avoid uploading big files that rarely change"
  [server-url resource-nodes-by-upload-path evaluation-context]
  (let [items (mapv (fn [[upload-path resource-node]]
                      (let [key (g/node-value resource-node :sha256 evaluation-context)]
                        {:path upload-path :key key}))
                    resource-nodes-by-upload-path)
        json (json/write-str {:files items :version 1 :hashType "sha256"})
        request (make-cache-request server-url json)
        response (http/request request)]
    ; Make a list of all files we intend to upload
    ; Make request to server with json
    ; Returns a json document with the "cached" fields filled in
    (when (= 200 (:status response))
      (let [body (slurp (:body response))
            items (get (json/read-str body) "files")]
        items))))

(defn- make-cached-info-map
  "Parse the json doc into a mapping 'path' -> 'cached'"
  [ne-cache-info]
  (into {}
        (map (fn [info]
               [(get info "path")
                (get info "cached")]))
        ne-cache-info))

(defn- build-engine-archive ^File
  [server-url extender-platform sdk-version resource-nodes-by-upload-path evaluation-context]
  ;; NOTE:
  ;; sdk-version is likely to be nil unless you're running a bundled editor.
  ;; In this case things will only work correctly if you're running a local
  ;; build server, as it will fall back on using the DYNAMO_HOME env variable.
  ;; Otherwise, you will likely get an Internal Server Error response.
  (let [cc (DefaultClientConfig.)
        ;; TODO: Random errors without this... Don't understand why random!
        ;; For example No MessageBodyWriter for body part of type 'java.io.BufferedInputStream' and media type 'application/octet-stream"
        _ (.add (.getClasses cc) MultiPartWriter)
        _ (.add (.getClasses cc) InputStreamProvider)
        _ (.add (.getClasses cc) StringProvider)
        client (doto (Client/create cc)
                 (.setConnectTimeout (int connect-timeout-ms))
                 (.setReadTimeout (int read-timeout-ms)))
        api-root (.resource client (URI. server-url))
        extender-username (System/getenv "DM_EXTENDER_USERNAME")
        extender-password (System/getenv "DM_EXTENDER_PASSWORD")
        user-info (.getUserInfo (URI. server-url))
        build-resource (.path api-root (build-url extender-platform sdk-version))
        builder (.getRequestBuilder build-resource)
        ne-cache-info (query-cached-files server-url resource-nodes-by-upload-path evaluation-context)
        ne-cache-info-map (make-cached-info-map ne-cache-info)]
    (.accept builder #^"[Ljavax.ws.rs.core.MediaType;" (into-array MediaType []))
    (if (not-empty user-info)
        (.header builder "Authorization" (str "Basic " (.encodeToString (Base64/getEncoder) (.getBytes user-info)))))
    (if (and (not-empty extender-username) (not-empty extender-password))
        (.header builder "Authorization" (str "Basic " (.encodeToString (Base64/getEncoder) (.getBytes (str extender-username ":" extender-password))))))
    (with-open [form (FormDataMultiPart.)]
      ; upload the file to the server, basically telling it what we are sending (and what we aren't)
      (.bodyPart form (StreamDataBodyPart. "ne-cache-info.json" (io/input-stream (.getBytes ^String (json/write-str {:files ne-cache-info})))))
      (doseq [[upload-path node] (sort-by first resource-nodes-by-upload-path)]
        ; If the file is not cached on the server, then we upload it
        (if (not (get ne-cache-info-map upload-path))
          (.bodyPart form (StreamDataBodyPart. upload-path (resource-node-content-stream node evaluation-context)))))
      (let [^ClientResponse cr (.post ^WebResource$Builder (.type builder MediaType/MULTIPART_FORM_DATA_TYPE) ClientResponse form)
            status (.getStatus cr)]
        (if (= 200 status)
          (let [engine-archive (fs/create-temp-file! "defold-engine" ".zip")]
            (io/copy (.getEntityInputStream cr) engine-archive)
            engine-archive)
          (let [log (.getEntity cr String)]
            (throw (engine-build-errors/build-error extender-platform status log))))))))

(defn get-build-server-url
  ^String [prefs]
  (prefs/get-prefs prefs "extensions-server" defold-build-server-url))

;; Note: When we do bundling for Android via the editor, we need add
;;       [["android" "proguard"] "_app/app.pro"] to the returned table.
(defn- global-resource-nodes-by-upload-path [project evaluation-context]
 (let [project-settings (g/node-value project :settings evaluation-context)]
   (into {}
         (keep (fn [[[section key] target]]
                 (when-let [resource (get project-settings [section key])]
                   (let [resource-node (project/get-resource-node project resource evaluation-context)]
                     (if (some-> resource-node (g/node-value :resource evaluation-context) resource/exists?)
                       [target resource-node]
                       (throw (engine-build-errors/missing-resource-error "Missing Native Extension Resource"
                                                                          (resource/proj-path resource)
                                                                          (project/get-resource-node project "/game.project" evaluation-context)))))))
               [[["native_extension" "app_manifest"] "_app/app.manifest"]]))))

(defn- get-ne-platform [platform]
  (case platform
    "x86_64-macos" "x86_64-osx"
    platform))

(defn- get-main-manifest-section-and-key [platform]
   (case platform
     "armv7-android" ["android" "manifest"]
     "arm64-android" ["android" "manifest"]
     "arm64-ios"     ["ios" "infoplist"]
     "armv7-ios"     ["ios" "infoplist"]
     "x86_64-osx"    ["osx" "infoplist"]
     "js-web"        ["html5" "htmlfile"]
     "wasm-web"      ["html5" "htmlfile"]))

(defn- get-main-manifest-name [ne-platform]
  (case ne-platform
    "armv7-android" "AndroidManifest.xml"
    "arm64-android" "AndroidManifest.xml"
    "arm64-ios"     "Info.plist"
    "armv7-ios"     "Info.plist"
    "x86_64-osx"    "Info.plist"
    "js-web"        "engine_template.html"
    "wasm-web"      "engine_template.html"
    nil))

(defn- get-main-manifest-file-upload-resource [project evaluation-context platform]
  (let [ne-platform (get-ne-platform platform)
        target-path  (get-main-manifest-name ne-platform)]
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

(defn has-extensions? [project evaluation-context]
  (not (empty? (merge (extension-roots project evaluation-context)
                      (global-resource-nodes-by-upload-path project evaluation-context)))))

(defn get-engine-archive [project evaluation-context platform build-server-url]
  (if-not (supported-platform? platform)
    (throw (engine-build-errors/unsupported-platform-error platform))
    (let [extender-platform (get-in extender-platforms [platform :platform])
          project-directory (workspace/project-path (project/workspace project evaluation-context) evaluation-context)
          cache-dir (cache-dir project-directory)
          resource-nodes-by-upload-path (merge (global-resource-nodes-by-upload-path project evaluation-context)
                                               (extension-resource-nodes-by-upload-path project evaluation-context platform)
                                               (get-main-manifest-file-upload-resource project evaluation-context platform))
          sdk-version (system/defold-engine-sha1)
          key (cache-key extender-platform sdk-version (map second (sort-by first resource-nodes-by-upload-path)) evaluation-context)]
      (if-let [cached-archive (cached-engine-archive cache-dir extender-platform key)]
        {:id {:type :custom :version key} :cached true :engine-archive cached-archive :extender-platform extender-platform}
        (let [temp-archive (build-engine-archive build-server-url extender-platform sdk-version resource-nodes-by-upload-path evaluation-context)
              engine-archive (cache-engine-archive! cache-dir extender-platform key temp-archive)]
          {:id {:type :custom :version key} :engine-archive engine-archive :extender-platform extender-platform})))))
