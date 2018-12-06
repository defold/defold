(ns editor.engine.native-extensions
  (:require
   [clojure.java.io :as io]
   [clojure.data.json :as json]
   [dynamo.graph :as g]
   [editor.prefs :as prefs]
   [editor.defold-project :as project]
   [editor.engine.build-errors :as engine-build-errors]
   [editor.fs :as fs]
   [editor.library :as library]
   [editor.resource :as resource]
   [editor.system :as system]
   [editor.workspace :as workspace]
   [util.http-client :as http])
  (:import
   (java.io File)
   (java.net URI)
   (java.util.zip ZipFile)
   (java.security MessageDigest)
   (javax.ws.rs.core MediaType)
   (org.apache.commons.codec.binary Hex)
   (org.apache.commons.codec.digest DigestUtils)
   (org.apache.commons.io IOUtils)
   (com.defold.editor Platform)
   (com.sun.jersey.api.client.config DefaultClientConfig)
   (com.sun.jersey.api.client Client ClientResponse WebResource$Builder)
   (com.sun.jersey.core.impl.provider.entity InputStreamProvider StringProvider)
   (com.sun.jersey.multipart FormDataMultiPart)
   (com.sun.jersey.multipart.impl MultiPartWriter)
   (com.sun.jersey.multipart.file StreamDataBodyPart)))

(set! *warn-on-reflection* true)

(def ^:const defold-build-server-url "https://build.defold.com")
(def ^:const connect-timeout-ms (* 30 1000))
(def ^:const read-timeout-ms (* 10 60 1000))

;;; Caching

(defn- hash-resources! ^MessageDigest
  [^MessageDigest md resource-nodes evaluation-context]
  (run! #(DigestUtils/updateDigest md ^String (g/node-value % :sha256 evaluation-context))
        resource-nodes)
  md)

(defn- cache-key
  [^String platform ^String sdk-version resource-nodes evaluation-context]
  (-> (DigestUtils/getSha256Digest)
      (DigestUtils/updateDigest platform)
      (DigestUtils/updateDigest (or sdk-version ""))
      (hash-resources! resource-nodes evaluation-context)
      (.digest)
      (Hex/encodeHexString)))

(defn- cache-file ^File
  [cache-dir platform]
  (doto (io/file cache-dir platform "build.zip")
    (fs/create-parent-directories!)))

(defn- cache-file-hash ^File
  [cache-dir platform]
  (doto (io/file cache-dir platform "build.hash")
    (fs/create-parent-directories!)))

(defn- cache-dir ^File
  [project-path]
  (doto (io/file project-path ".internal" "cache" "engine-archives")
    (fs/create-parent-directories!)))

(defn- cached-engine-archive
  [cache-dir platform key]
  (let [cache-file (cache-file cache-dir platform)
        cache-file-hash (cache-file-hash cache-dir platform)]
    (when (and (.exists cache-file)
               (.exists cache-file-hash)
               (= key (slurp cache-file-hash)))
      cache-file)))

(defn- cache-engine-archive!
  [cache-dir platform key ^File engine-archive]
  (let [cache-file (cache-file cache-dir platform)
        cache-file-hash (cache-file-hash cache-dir platform)]
    (fs/move-file! engine-archive cache-file)
    (spit cache-file-hash key)
    cache-file))


;;; Extension discovery/processing

(def ^:private extender-platforms
  {(.getPair Platform/X86Darwin)    {:platform      "x86-osx"
                                     :library-paths #{"osx" "x86-osx"}}
   (.getPair Platform/X86_64Darwin) {:platform      "x86_64-osx"
                                     :library-paths #{"osx" "x86_64-osx"}}
   (.getPair Platform/Armv7Darwin)  {:platform      "armv7-ios"
                                     :library-paths #{"ios" "armv7-ios"}}
   (.getPair Platform/Arm64Darwin)  {:platform      "arm64-ios"
                                     :library-paths #{"ios" "arm64-ios"}}
   (.getPair Platform/Armv7Android) {:platform      "armv7-android"
                                     :library-paths #{"android" "armv7-android"}}
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
   ["src"]
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
  [platform sdk-version]
  (format "/build/%s/%s" platform (or sdk-version "")))

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

;; The server expects at least path+key
;; The cached field is always written from server
(defn- make-server-cache-item [path key cached]
  {:path path :key key :cached cached})

;; Asks the server what files it already has
;; This is to avoid uploading big files that rarely change
(defn- query-cached-files
  [server-url resource-nodes-by-upload-path]
  (let [paths (map first resource-nodes-by-upload-path)
        keys (map #(g/node-value % :sha256) (map second resource-nodes-by-upload-path))
        items (map #(make-server-cache-item (first %) (second %) false) (map vector paths keys))
        json (json/write-str {:files items})
        request (make-cache-request server-url json)
        response (http/request request)]
    ; Make a list of all files we intend to upload
    ; Make request to server with json
    ; Returns a json document with the "cached" fields filled in
    (when (= 200 (:status response))
      (let [body (slurp (:body response))
            items (get (json/read-str body) "files")]
        items))))

;; Parse the json doc into a mapping "path"->"cached"
(defn- make-cached-info-map
  [ne-cache-info]
  (zipmap (map #(get % "path") ne-cache-info) (map #(get % "cached") ne-cache-info)))

(defn- build-engine-archive ^File
  [server-url platform sdk-version resource-nodes-by-upload-path evaluation-context]
  ;; NOTE:
  ;; sdk-version is likely to be nil unless you're running a bundled editor.
  ;; In this case things will only work correctly if you're running a local
  ;; build server, as it will fall back on using the DYNAMO_HOME env variable.
  ;; Otherwise, you will likely get an Internal Server Error response.
  (let [cc (DefaultClientConfig.)
        ;; TODO: Random errors wihtout this... Don't understand why random!
        ;; For example No MessageBodyWriter for body part of type 'java.io.BufferedInputStream' and media type 'application/octet-stream"
        _ (.add (.getClasses cc) MultiPartWriter)
        _ (.add (.getClasses cc) InputStreamProvider)
        _ (.add (.getClasses cc) StringProvider)
        client (doto (Client/create cc)
                 (.setConnectTimeout (int connect-timeout-ms))
                 (.setReadTimeout (int read-timeout-ms)))
        api-root (.resource client (URI. server-url))
        build-resource (.path api-root (build-url platform sdk-version))
        builder (.accept build-resource #^"[Ljavax.ws.rs.core.MediaType;" (into-array MediaType []))
        ne-cache-info (query-cached-files server-url resource-nodes-by-upload-path)
        ne-cache-info-map (make-cached-info-map ne-cache-info)]
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
            (throw (engine-build-errors/build-error platform status log))))))))

(defn- find-or-build-engine-archive
  [cache-dir server-url platform sdk-version resource-nodes-by-upload-path evaluation-context]
  (let [key (cache-key platform sdk-version (map second (sort-by first resource-nodes-by-upload-path)) evaluation-context)]
    (or (cached-engine-archive cache-dir platform key)
        (let [engine-archive (build-engine-archive server-url platform sdk-version resource-nodes-by-upload-path evaluation-context)]
          (cache-engine-archive! cache-dir platform key engine-archive)))))

(defn- ensure-empty-unpack-dir!
  [project-path platform]
  (let [dir (io/file project-path "build" platform)]
    (fs/delete-directory! dir {:missing :ignore})
    (fs/create-directories! dir)
    dir))

(def ^:private dmengine-dependencies
  {"x86_64-win32" #{"OpenAL32.dll" "wrap_oal.dll"}
   "x86-win32"    #{"OpenAL32.dll" "wrap_oal.dll"}})

(defn- copy-dmengine-dependencies!
  [unpack-dir platform]
  (let [bundled-engine-dir (io/file (system/defold-unpack-path) platform "bin")]
    (doseq [dep (dmengine-dependencies platform)]
      (fs/copy! (io/file bundled-engine-dir dep) (io/file unpack-dir dep)))))

(defn- unpack-dmengine
  [^File engine-archive project-path platform]
  (with-open [zip-file (ZipFile. engine-archive)]
    (let [suffix (.getExeSuffix (Platform/getHostPlatform))
          dmengine-entry (.getEntry zip-file (format "dmengine%s" suffix))
          stream (.getInputStream zip-file dmengine-entry)
          unpack-dir (ensure-empty-unpack-dir! project-path platform)
          engine-file (io/file unpack-dir (format "dmengine%s" suffix))]
      (io/copy stream engine-file)
      (fs/set-executable! engine-file)
      (copy-dmengine-dependencies! unpack-dir platform)
      engine-file)))

(defn get-build-server-url
  ^String [prefs]
  (prefs/get-prefs prefs "extensions-server" defold-build-server-url))

(defn- get-app-manifest-resource [project-settings]
  (get project-settings ["native_extension" "app_manifest"]))

(defn- global-resource-nodes-by-upload-path [project evaluation-context]
  (if-some [app-manifest-resource (get-app-manifest-resource (g/node-value project :settings evaluation-context))]
    (let [resource-node (project/get-resource-node project app-manifest-resource evaluation-context)]
      (if (some-> resource-node (g/node-value :resource evaluation-context) resource/exists?)
        {"_app/app.manifest" resource-node}
        (throw (engine-build-errors/missing-resource-error "Native Extension App Manifest"
                                                           (resource/proj-path app-manifest-resource)
                                                           (project/get-resource-node project "/game.project" evaluation-context)))))
    {}))

(defn- resource-node-upload-path [resource-node evaluation-context]
  (fs/without-leading-slash (resource/proj-path (g/node-value resource-node :resource evaluation-context))))

(defn- extension-resource-nodes-by-upload-path [project evaluation-context platform]
  (into {}
        (map (juxt #(resource-node-upload-path % evaluation-context) identity))
        (extension-resource-nodes project evaluation-context platform)))

(defn has-extensions? [project evaluation-context]
  (not (empty? (merge (extension-roots project evaluation-context)
                      (global-resource-nodes-by-upload-path project evaluation-context)))))

(defn get-engine
  [project evaluation-context platform build-server]
  (if-not (supported-platform? platform)
    (throw (engine-build-errors/unsupported-platform-error platform))
    (let [project-path (workspace/project-path (project/workspace project evaluation-context) evaluation-context)]
      (unpack-dmengine (find-or-build-engine-archive (cache-dir project-path)
                                                     build-server
                                                     (get-in extender-platforms [platform :platform])
                                                     (system/defold-engine-sha1)
                                                     (merge (global-resource-nodes-by-upload-path project evaluation-context)
                                                            (extension-resource-nodes-by-upload-path project evaluation-context platform))
                                                     evaluation-context)
                       project-path
                       platform))))
