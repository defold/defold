(ns editor.engine.native-extensions
  (:require
   [clojure.java.io :as io]
   [clojure.string :as str]
   [dynamo.graph :as g]
   [editor.prefs :as prefs]
   [editor.defold-project :as project]
   [editor.resource :as resource]
   [editor.system :as system]
   [editor.workspace :as workspace])
  (:import
   (java.io File)
   (java.net URI)
   (java.util.zip ZipFile)
   (java.security MessageDigest)
   (javax.ws.rs.core MediaType)
   (org.apache.commons.codec.binary Hex)
   (org.apache.commons.codec.digest DigestUtils)
   (org.apache.commons.io FileUtils FilenameUtils IOUtils)
   (com.sun.jersey.api.client Client ClientResponse WebResource WebResource$Builder)
   (com.sun.jersey.api.client.config ClientConfig DefaultClientConfig)
   (com.sun.jersey.core.impl.provider.entity InputStreamProvider StringProvider)
   (com.sun.jersey.multipart FormDataMultiPart)
   (com.sun.jersey.multipart.impl MultiPartWriter)
   (com.sun.jersey.multipart.file FileDataBodyPart StreamDataBodyPart)
   (com.defold.editor Platform)))

(set! *warn-on-reflection* true)

(def ^:const defold-build-server-url "https://build.defold.com")

;;; Caching

(defn- hash-resources! ^MessageDigest
  [^MessageDigest md resource-nodes]
  (run! #(DigestUtils/updateDigest md ^String (g/node-value % :sha256))
        (sort resource-nodes))
  md)

(defn- cache-key
  [^String platform ^String sdk-version resource-nodes]
  (-> (DigestUtils/getSha256Digest)
      (DigestUtils/updateDigest platform)
      (DigestUtils/updateDigest (or sdk-version ""))
      (hash-resources! resource-nodes)
      (.digest)
      (Hex/encodeHexString)))

(defn- cache-file ^File
  [cache-dir platform]
  (doto (io/file cache-dir platform "build.zip")
    (io/make-parents)))

(defn- cache-file-hash ^File
  [cache-dir platform]
  (doto (io/file cache-dir platform "build.hash")
    (io/make-parents)))

(defn- cache-dir ^File
  [project-path]
  (doto (io/file project-path ".internal" "cache" "engine-archives")
    (io/make-parents)))

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
    (.renameTo engine-archive cache-file)
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
                                     :library-paths #{"android" "armv7-android"}}})

#_(def ^:private not-yet-supported-extender-platforms
  {(.getPair Platform/X86Win32)     {:platform      "x86-windows"
                                     :library-paths #{"windows" "x86-windows"}}
   (.getPair Platform/X86_64Win32)  {:platform      "x86_64-windows"
                                     :library-paths #{"windows" "x86_64-windows"}}
   (.getPair Platform/X86Linux)     {:platform      "x86-linux"
                                     :library-paths #{"linux" "x86-linux"}}
   (.getPair Platform/X86_64Linux)  {:platform      "x86_64-linux"
                                     :library-paths #{"linux" "x86_64-linux"}}
   (.getPair Platform/JsWeb)        {:platform      "js-web"
                                     :library-paths #{"web" "js-web"}}})

(def ^:private common-extension-paths
  [["ext.manifest"]
   ["include"]
   ["src"]
   ["lib" "common"]])

(defn- platform-extension-paths
  [platform]
  (into common-extension-paths (map #(vector "lib" %) (get-in extender-platforms [platform :library-paths]))))

(defn- extension-manifest?
  [resource]
  (= "ext.manifest" (resource/resource-name resource)))

(defn- extension-manifests
  [project]
  (->> (g/node-value project :resources)
       (filter extension-manifest?)
       (seq)))

(defn- extension-manifest-node-ids [project]
  (keep (partial project/get-resource-node project) (extension-manifests project)))

(defn extension-root?
  [resource]
  (some extension-manifest? (resource/children resource)))

(defn extension-roots
  [project]
  (->> (g/node-value project :resources)
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
  [project roots platform]
  (let [paths (platform-extension-paths platform)]
    (->> (for [root roots
               path paths
               :let [resource (resource-by-path root path)]
               :when resource]
           resource)
         (mapcat resource/resource-seq)
         (filter #(= :file (resource/source-type %)))
         (map (fn [resource]
                (project/get-resource-node project resource))))))

;;; Extender API

(defn- build-url
  [platform sdk-version]
  (format "/build/%s/%s" platform (or sdk-version "")))

(defn- resource-content-stream ^java.io.InputStream
  [resource-node]
  (if-let [content (some-> (g/node-value resource-node :save-data) :content)]
    (IOUtils/toInputStream ^String content "UTF-8")
    (io/input-stream (g/node-value resource-node :resource))))

(defn- without-leading-slash [^String s]
  (if (.startsWith s "/")
    (subs s 1)
    s))

(def ^:private proj-path-without-leading-slash (comp without-leading-slash resource/proj-path))

;;; Log parsing

(defn- try-parse-error-causes [project log re match->cause resolve-data-fn]
  (assert (fn? match->cause))
  (assert (or (nil? resolve-data-fn) (fn? resolve-data-fn)))
  (when-let [matches (not-empty (re-seq re log))]
    (let [resolve-data (when (some? resolve-data-fn) (resolve-data-fn project))]
      (mapv #(match->cause project % resolve-data) matches))))

(def ^:private invalid-lib-re #"(?m)^Invalid lib in extension '(.+?)'.*$")
(def ^:private manifest-ext-name-re1 #"(?m)^name:\s*\"(.+)\"\s*$") ; Double-quoted name
(def ^:private manifest-ext-name-re2 #"(?m)^name:\s*'(.+)'\s*$") ; Single-quoted name

(defn- try-parse-manifest-ext-name [resource]
  (when (satisfies? io/IOFactory resource)
    (let [file-contents (slurp resource)]
      (second (or (re-find manifest-ext-name-re1 file-contents)
                  (re-find manifest-ext-name-re2 file-contents))))))

(defn- invalid-lib-match->cause [project [all manifest-ext-name] manifest-ext-resources-by-name]
  (assert (string? (not-empty all)))
  (assert (string? (not-empty manifest-ext-name)))
  (let [manifest-ext-resource (manifest-ext-resources-by-name manifest-ext-name)
        node-id (project/get-resource-node project manifest-ext-resource)]
    {:_node-id node-id
     :message all}))

(defn- invalid-lib-match-resolve-data [project]
  (into {}
        (keep (fn [resource]
                (when-let [manifest-ext-name (try-parse-manifest-ext-name resource)]
                  [manifest-ext-name resource])))
        (extension-manifests project)))

(def ^:private gcc-re #"(?m)^/tmp/upload[0-9]+/(.+):([0-9]+):([0-9]+):\s+(.+)$")

(defn- gcc-match->cause [project [_all path line _column message] _resolve-data]
  (assert (string? (not-empty path)))
  (assert (string? (not-empty message)))
  (let [line-number (Integer/parseInt line)
        node-id (project/get-resource-node project (str "/" path))]
    {:_node-id node-id
     :value (reify clojure.lang.IExceptionInfo
              (getData [_]
                {:line line-number}))
     :message message}))

(defn has-extensions? [project]
  (not (empty? (extension-roots project))))

(defn supported-platform? [platform]
  (contains? extender-platforms platform))

(defn- unsupported-platform-error [platform]
  (ex-info (str "Unsupported platform " platform)
           {:type ::unsupported-platform-error
            :platform platform}))

(defn- unsupported-platform-error? [exception]
  (= ::unsupported-platform-error (:type (ex-data exception))))

(defn unsupported-platform-error-causes [project]
  [{:_node-id (first (extension-manifest-node-ids project))
    :message "Native Extensions are not yet supported for the target platform."}])

(defn- generic-extension-error-causes
  "Splits log lines across multiple build error entries, since the control
  we use currently does not cope well with multi-line strings."
  [project log]
  (let [node-id (first (extension-manifest-node-ids project))]
    (or (not-empty (into []
                         (comp (filter not-empty)
                               (map (fn [log-line]
                                      {:_node-id node-id
                                       :message log-line})))
                         (str/split-lines log)))
        [{:_node-id node-id
          :message "An error occurred while building Native Extensions, but the server provided no information."}])))

(defn log-error-causes [project log]
  (or (try-parse-error-causes project log invalid-lib-re invalid-lib-match->cause invalid-lib-match-resolve-data)
      (try-parse-error-causes project log gcc-re gcc-match->cause nil)
      (generic-extension-error-causes project log)))

(defn- build-error-causes [project exception]
  (if-let [log (not-empty (:log (ex-data exception)))]
    (log-error-causes project log)
    []))

(defn- build-error? [exception]
  (= ::build-error (:type (ex-data exception))))

(defn handle-error! [render-error! project e]
  (cond
    (unsupported-platform-error? e)
    (do (render-error! {:causes (unsupported-platform-error-causes project)})
        true)

    (build-error? e)
    (do (render-error! {:causes (build-error-causes project e)})
        true)

    :else
    false))

;;; Building

(defn- build-engine-archive ^File
  [server-url platform sdk-version resource-nodes]
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
        client (Client/create cc)
        api-root (.resource client (URI. server-url))
        build-resource (.path api-root (build-url platform sdk-version))
        builder (.accept build-resource #^"[Ljavax.ws.rs.core.MediaType;" (into-array MediaType []))]
    (with-open [form (FormDataMultiPart.)]
      (doseq [node resource-nodes]
        (let [resource (g/node-value node :resource)]
          (.bodyPart form (StreamDataBodyPart. (proj-path-without-leading-slash resource) (resource-content-stream node)))))
      (let [^ClientResponse cr (.post ^WebResource$Builder (.type builder MediaType/MULTIPART_FORM_DATA_TYPE) ClientResponse form)]
        (case (.getStatus cr)
          200 (let [f (doto (File/createTempFile "defold-engine" ".zip")
                        (.deleteOnExit))]
                (FileUtils/copyInputStreamToFile (.getEntityInputStream cr) f)
                f)

          422 (let [log (.getEntity cr String)]
                (throw (ex-info (format "Compilation error: %s" log)
                                {:type ::build-error
                                 :status (.getStatus cr)
                                 :log log})))

          (throw (ex-info (format "Failed to build engine: %s, status: %d" (.getEntity cr String) (.getStatus cr))
                          {:status (.getStatus cr)})))))))

(defn- find-or-build-engine-archive
  [cache-dir server-url platform sdk-version resource-nodes]
  (let [key (cache-key platform sdk-version resource-nodes)]
    (or (cached-engine-archive cache-dir platform key)
        (let [engine-archive (build-engine-archive server-url platform sdk-version resource-nodes)]
          (cache-engine-archive! cache-dir platform key engine-archive)))))

(defn- unpack-dmengine
  [^File engine-archive]
  (with-open [zip-file (ZipFile. engine-archive)]
    (let [dmengine-entry (.getEntry zip-file "dmengine")
          stream (.getInputStream zip-file dmengine-entry)
          engine-file (doto (File/createTempFile "dmengine" "")
                        (.deleteOnExit))]
      (FileUtils/copyInputStreamToFile stream engine-file)
      (doto engine-file
        (.setExecutable true)))))

(defn get-build-server-url
  ^String [prefs]
  (prefs/get-prefs prefs "extensions-server" defold-build-server-url))

(defn get-engine
  [project roots platform build-server]
  (if-not (supported-platform? platform)
    (throw (unsupported-platform-error platform))
    (unpack-dmengine (find-or-build-engine-archive (cache-dir (workspace/project-path (project/workspace project)))
                                                   build-server
                                                   (get-in extender-platforms [platform :platform])
                                                   (system/defold-sha1)
                                                   (extension-resource-nodes project roots platform)))))
