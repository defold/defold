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
   (.getPair Platform/X86Win32)     {:platform      "x86-windows"
                                     :library-paths #{"windows" "x86-windows"}}
   (.getPair Platform/X86_64Win32)  {:platform      "x86_64-windows"
                                     :library-paths #{"windows" "x86_64-windows"}}
   (.getPair Platform/X86Linux)     {:platform      "x86-linux"
                                     :library-paths #{"linux" "x86-linux"}}
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

(defn extension-root?
  [resource]
  (some #(= "ext.manifest" (resource/resource-name %)) (resource/children resource)))

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

          422 (throw (ex-info (format "Compilation error: %s" (.getEntity cr String))
                              {:status (.getStatus cr)}))

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

(defn get-engine
  [project roots platform build-server]
  (let [extender-platform (get-in extender-platforms [platform :platform])
        engine-archive (find-or-build-engine-archive (cache-dir (workspace/project-path (project/workspace project)))
                                                     build-server
                                                     extender-platform
                                                     (system/defold-sha1)
                                                     (extension-resource-nodes project roots platform))]
    (unpack-dmengine engine-archive)))
