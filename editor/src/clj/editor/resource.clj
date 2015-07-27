(ns editor.resource
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.core :as core])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils IOUtils]))

(defprotocol ResourceListener
  (handle-changes [this changes]))

(defprotocol Resource
  (resource-type [this])
  (source-type [this])
  (read-only? [this])
  (path [this])
  (abs-path [this])
  (proj-path [this])
  (url [this])
  (resource-name [this])
  (workspace [this])
  (resource-hash [this]))

(defn- relative-path [^File f1 ^File f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

(defrecord FileResource [workspace ^File file children]
  Resource
  (resource-type [this] (get (g/node-value workspace :resource-types) (FilenameUtils/getExtension (.getPath file))))
  (source-type [this] (if (.isFile file) :file :folder))
  (read-only? [this] (not (.canWrite file)))
  (path [this] (if (= "" (.getName file)) "" (relative-path (File. ^String (g/node-value workspace :root)) file)))
  (abs-path [this] (.getAbsolutePath  file))
  (proj-path [this] (if (= "" (.getName file)) "" (str "/" (path this))))
  (url [this] (relative-path (File. ^String (g/node-value workspace :root)) file))
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
  (fn [{:keys [workspace file children]}]
    (FileResource. workspace (File. file) children))))

(core/register-write-handler!
 FileResource
 (transit/write-handler
  (constantly "file-resource")
  (fn [^FileResource r]
    {:workspace (:workspace r)
     :file      (.getPath (:file r))
     :children  (:children r)})))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "FileResource{:workspace %s :file %s :children %s}" (:workspace file-resource) (:file file-resource) (str (:children file-resource)))))

(defrecord MemoryResource [workspace resource-type data]
  Resource
  (resource-type [this] resource-type)
  (source-type [this] :file)
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

(core/register-record-type MemoryResource)

(defmethod print-method MemoryResource [memory-resource ^java.io.Writer w]
  (.write w (format "MemoryResource{:workspace %s :data %s}" (:workspace memory-resource) (:data memory-resource))))

(defrecord ZipResource [workspace name path data children]
  Resource
  (resource-type [this] (get (g/node-value workspace :resource-types) (FilenameUtils/getExtension name)))
  (source-type [this] (if (zero? (count children)) :file :folder))
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

(core/register-record-type ZipResource)

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

(defn- load-zip [url]
  (with-open [zip (ZipInputStream. (io/input-stream url))]
    (loop [entries []]
      (let [e (.getNextEntry zip)]
        (if-not e
          entries
          (recur (if (.isDirectory e)
                   entries
                   (conj entries {:name (last (string/split (.getName e) #"/"))
                                  :path (.getName e)
                                  :buffer (read-zip-entry zip e)}))))))))

(defn- ->zip-resources [workspace path [key val]]
  (let [path' (str path "/" key)]
    (if (:path val)
      (ZipResource. workspace (:name val) (:path val) (:buffer val) nil)
      (ZipResource. workspace key path' nil (mapv (fn [x] (->zip-resources workspace path' x)) val)))))

(defn make-zip-tree [workspace file-name]
  (let [entries (load-zip file-name)]
    (->> (reduce (fn [acc node] (assoc-in acc (string/split (:path node) #"/") node)) {} entries)
      (mapv (fn [x] (->zip-resources workspace "" x))))))
