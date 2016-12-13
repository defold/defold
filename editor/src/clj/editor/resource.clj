(ns editor.resource
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.core :as core]
            [editor.handler :as handler])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils IOUtils]))

(set! *warn-on-reflection* true)

(defprotocol ResourceListener
  (handle-changes [this changes render-progress!]))

(defprotocol Resource
  (children [this])
  (ext [this])
  (resource-type [this])
  (source-type [this])
  (exists? [this])
  (read-only? [this])
  (path [this])
  (abs-path ^String [this])
  (proj-path ^String [this])
  (url [this])
  (resource-name ^String [this])
  (workspace [this])
  (resource-hash [this]))

(defn- ->unix-seps ^String [^String path]
  (FilenameUtils/separatorsToUnix path))

(defn relative-path [^File f1 ^File f2]
  (->unix-seps (.toString (.relativize (.toPath f1) (.toPath f2)))))

(defn file->proj-path [^File project-path ^File f]
  (try
    (str "/" (relative-path project-path f))
    (catch IllegalArgumentException e
      nil)))

(defrecord FileResource [workspace root ^File file children]
  Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension (.getPath file)))
  (resource-type [this] (get (g/node-value workspace :resource-types) (ext this)))
  (source-type [this] (if (.isDirectory file) :folder :file))
  (exists? [this] (.exists file))
  (read-only? [this] (not (.canWrite file)))
  (path [this] (if (= "" (.getName file)) "" (relative-path (File. ^String root) file)))
  (abs-path [this] (.getAbsolutePath  file))
  (proj-path [this] (if (= "" (.getName file)) "" (str "/" (path this))))
  (url [this] (relative-path (File. ^String root) file))
  (resource-name [this] (.getName file))
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream file opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream file opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(core/register-read-handler!
 "file-resource"
 (transit/read-handler
  (fn [{:keys [workspace ^String file children]}]
    (FileResource. workspace (g/node-value workspace :root) (File. file) children))))

(core/register-write-handler!
 FileResource
 (transit/write-handler
  (constantly "file-resource")
  (fn [^FileResource r]
    {:workspace (:workspace r)
     :file      (.getPath ^File (:file r))
     :children  (:children r)})))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "FileResource{:workspace %s :file %s :children %s}" (:workspace file-resource) (:file file-resource) (str (:children file-resource)))))

(defrecord MemoryResource [workspace ext data]
  Resource
  (children [this] nil)
  (ext [this] ext)
  (resource-type [this] (get (g/node-value workspace :resource-types) ext))
  (source-type [this] :file)
  (exists? [this] true)
  (read-only? [this] false)
  (path [this] nil)
  (abs-path [this] nil)
  (proj-path [this] nil)
  (url [this] nil)
  (resource-name [this] nil)
  (workspace [this] workspace)
  (resource-hash [this] (hash data))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (IOUtils/toInputStream ^String (:data this)) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (.toCharArray ^String (:data this)) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(core/register-record-type! MemoryResource)

(defmethod print-method MemoryResource [memory-resource ^java.io.Writer w]
  (.write w (format "MemoryResource{:workspace %s :data %s}" (:workspace memory-resource) (:data memory-resource))))

(defn make-memory-resource [workspace resource-type data]
  (MemoryResource. workspace (:ext resource-type) data))

(defrecord ZipResource [workspace name path data children]
  Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension name))
  (resource-type [this] (get (g/node-value workspace :resource-types) (ext this)))
  (source-type [this] (if (zero? (count children)) :file :folder))
  (exists? [this] (not (nil? data)))
  (read-only? [this] true)
  (path [this] path)
  (abs-path [this] nil)
  (proj-path [this] (str "/" path))
  ; TODO
  (url [this] nil)
  (resource-name [this] name)
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (:data this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (throw (Exception. "Zip resources are read-only")))
  (io/make-writer        [this opts] (throw (Exception. "Zip resources are read-only"))))

(core/register-record-type! ZipResource)

(defmethod print-method ZipResource [zip-resource ^java.io.Writer w]
  (.write w (format "ZipResource{:workspace %s :path %s :children %s}" (:workspace zip-resource) (:path zip-resource) (str (:children zip-resource)))))

(defn- read-zip-entry [^ZipInputStream zip ^ZipEntry e]
  (let [os (ByteArrayOutputStream.)
        buf (byte-array (* 1024 16))]
    (loop [n (.read zip buf 0 (count buf))]
     (when (> n 0)
       (.write os buf 0 n)
       (recur (.read zip buf 0 (count buf)))))
    (.toByteArray os)))

(defn- outside-base-path? [base-path ^ZipEntry entry]
  (and (seq base-path) (not (.startsWith (->unix-seps (.getName entry)) (str base-path "/")))))

(defn- path-relative-base [base-path ^ZipEntry entry]
  (let [entry-name (->unix-seps (.getName entry))]
    (if (seq base-path)
      (.replaceFirst entry-name (str base-path "/") "")
      entry-name)))

(defn- load-zip [url base-path]
  (when-let [stream (and url (io/input-stream url))]
    (with-open [zip (ZipInputStream. stream)]
      (loop [entries []]
        (let [e (.getNextEntry zip)]
          (if-not e
            entries
            (recur (if (or (.isDirectory e) (outside-base-path? base-path e))
                     entries
                     (conj entries {:name (FilenameUtils/getName (.getName e))
                                    :path (path-relative-base base-path e)
                                    :buffer (read-zip-entry zip e)})))))))))

(defn- ->zip-resources [workspace path [key val]]
  (let [path' (if (string/blank? path) key (str path "/" key))]
    (if (:path val) ; i.e. we've reached an actual entry with name, path, buffer
      (ZipResource. workspace (:name val) (:path val) (:buffer val) nil)
      (ZipResource. workspace key path' nil (mapv (fn [x] (->zip-resources workspace path' x)) val)))))

(defn make-zip-tree
  ([workspace file-name]
   (make-zip-tree workspace file-name nil))
  ([workspace file-name base-path]
   (let [entries (load-zip file-name base-path)]
     (->> (reduce (fn [acc node] (assoc-in acc (string/split (:path node) #"/") node)) {} entries)
          (mapv (fn [x] (->zip-resources workspace "" x)))))))

(g/defnode ResourceNode
  (extern resource Resource (dynamic visible (g/constantly false))))

(defn- seq-children [resource]
  (seq (children resource)))

(defn resource-seq [root]
  (tree-seq seq-children seq-children root))

(defn resource-list-seq [resource-list]
  (apply concat (map resource-seq resource-list)))

(defn resource-map [roots]
  (into {} (map (juxt proj-path identity) roots)))

(defn resource->proj-path [resource]
  (if resource
    (proj-path resource)
    ""))

(g/deftype ResourceVec [(s/maybe (s/protocol Resource))])

(defn node-id->resource [node-id]
  (when (g/node-instance? ResourceNode node-id) (g/node-value node-id :resource)))

(defn temp-path [resource]
  (when (and resource (= :file (source-type resource)))
    (let [^File f (doto (File/createTempFile "tmp" (format ".%s" (ext resource)))
                    (.deleteOnExit))]
      (with-open [in (io/input-stream resource)
                  out (io/output-stream f)]
        (IOUtils/copy in out))
      (.getAbsolutePath f))))
