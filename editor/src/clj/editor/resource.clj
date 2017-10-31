(ns editor.resource
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.core :as core]
            [editor.fs :as fs]
            [editor.handler :as handler])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.nio.file FileSystem FileSystems PathMatcher]
           [java.net URL]
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
  (resource-name ^String [this])
  (workspace [this])
  (resource-hash [this])
  (openable? [this]))

(defn openable-resource? [value]
  ;; A resource is considered openable if its kind can be opened. Typically this
  ;; is a resource that is part of the project and is not a directory. Note
  ;; that the resource does not have to be openable in the Defold Editor - an
  ;; external application could be assigned to handle it. Before opening, you
  ;; must also make sure the resource exists.
  (and (satisfies? Resource value)
       (openable? value)))

(defn- ->unix-seps ^String [^String path]
  (FilenameUtils/separatorsToUnix path))

(defn relative-path [^File f1 ^File f2]
  (let [p1 (->unix-seps (str (.getAbsolutePath f1)))
        p2 (->unix-seps (str (.getAbsolutePath f2)))
        path (string/replace p2 p1 "")]
    (if (.startsWith path "/")
      (subs path 1)
      path)))

(defn file->proj-path [^File project-path ^File f]
  (try
    (str "/" (relative-path project-path f))
    (catch IllegalArgumentException e
      nil)))

(defn parent-proj-path [^String proj-path]
  (when-let [last-slash (string/last-index-of proj-path "/")]
    (subs proj-path 0 last-slash)))

;; Note! Used to keep a file here instead of path parts, but on
;; Windows (File. "test") equals (File. "Test") which broke
;; FileResource equality tests.
(defrecord FileResource [workspace ^String abs-path ^String project-path ^String name ^String ext source-type children]
  Resource
  (children [this] children)
  (ext [this] ext)
  (resource-type [this] (get (g/node-value workspace :resource-types) ext))
  (source-type [this] source-type)
  (exists? [this] (.exists (io/file this)))
  (read-only? [this] (not (.canWrite (io/file this))))
  (path [this] (if (= "" project-path) "" (subs project-path 1)))
  (abs-path [this] abs-path)
  (proj-path [this] project-path)
  (resource-name [this] name)
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))
  (openable? [this] (= :file source-type))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (io/file this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (io/file this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  io/Coercions
  (io/as-file [this] (File. abs-path)))

(defn make-file-resource [workspace ^String root ^File file children]
  (let [source-type (if (.isDirectory file) :folder :file)
        abs-path (.getAbsolutePath file)
        path (.getPath file)
        name (.getName file)
        project-path (if (= "" name) "" (str "/" (relative-path (File. root) (io/file path))))
        ext (FilenameUtils/getExtension path)]
    (FileResource. workspace abs-path project-path name ext source-type children)))

(defn file-resource? [resource]
  (instance? FileResource resource))

(core/register-read-handler!
  "file-resource"
  (transit/read-handler
    (fn [{:keys [workspace ^String abs-path ^String project-path ^String name ^String ext source-type children]}]
      (FileResource. workspace abs-path project-path name ext source-type children))))

(core/register-write-handler!
 FileResource
 (transit/write-handler
  (constantly "file-resource")
  (fn [^FileResource r]
    {:workspace (:workspace r)
     :abs-path (:abs-path r)
     :project-path (:project-path r)
     :name (:name r)
     :ext (:ext r)
     :source-type (:source-type r)
     :children (:children r)})))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "{:FileResource %s}" (pr-str (proj-path file-resource)))))

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
  (resource-name [this] nil)
  (workspace [this] workspace)
  (resource-hash [this] (hash data))
  (openable? [this] false)

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (IOUtils/toInputStream ^String (:data this)) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (.toCharArray ^String (:data this)) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(core/register-record-type! MemoryResource)

(defmethod print-method MemoryResource [memory-resource ^java.io.Writer w]
  (.write w (format "{:MemoryResource %s}" (pr-str (ext memory-resource)))))

(defn make-memory-resource [workspace resource-type data]
  (MemoryResource. workspace (:ext resource-type) data))

(defrecord ZipResource [workspace ^URL zip-url name path data children]
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
  (resource-name [this] name)
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))
  (openable? [this] (= :file (source-type this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (:data this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (throw (Exception. "Zip resources are read-only")))
  (io/make-writer        [this opts] (throw (Exception. "Zip resources are read-only")))

  io/Coercions
  (io/as-file [this] (when (= (.getPath zip-url) (.getFile zip-url)) (io/file (.getFile zip-url)))))

(core/register-record-type! ZipResource)

(core/register-read-handler!
 "zip-resource"
 (transit/read-handler
  (fn [{:keys [workspace ^String zip-url name path data children]}]
    (ZipResource. workspace (URL. zip-url) name path data children))))

(core/register-write-handler!
 ZipResource
 (transit/write-handler
  (constantly "zip-resource")
  (fn [^ZipResource r]
    {:workspace (:workspace r)
     :zip-url   (.toString ^URL (:zip-url r))
     :name      (:name r)
     :path      (:path r)
     :data      (:data r)     
     :children  (:children r)})))

(defmethod print-method ZipResource [zip-resource ^java.io.Writer w]
  (.write w (format "{:ZipResource %s}" (pr-str (proj-path zip-resource)))))

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

(defn- load-zip [file-or-url base-path]
  (when-let [stream (and file-or-url (io/input-stream file-or-url))]
    (with-open [zip (ZipInputStream. stream)]
      (loop [entries []]
        (let [e (.getNextEntry zip)]
          (if-not e
            entries
            (recur (if (or (.isDirectory e) (outside-base-path? base-path e))
                     entries
                     (conj entries {:name (FilenameUtils/getName (.getName e))
                                    :path (path-relative-base base-path e)
                                    :buffer (read-zip-entry zip e)
                                    :crc (.getCrc e)})))))))))

(defn- ->zip-resources [workspace zip-url path [key val]]
  (let [path' (if (string/blank? path) key (str path "/" key))]
    (if (:path val) ; i.e. we've reached an actual entry with name, path, buffer
      (ZipResource. workspace zip-url (:name val) (:path val) (:buffer val) nil)
      (ZipResource. workspace zip-url key path' nil (mapv (fn [x] (->zip-resources workspace zip-url path' x)) val)))))

(defn load-zip-resources
  ([workspace file-or-url]
   (load-zip-resources workspace file-or-url nil))
  ([workspace file-or-url base-path]
   (let [entries (load-zip file-or-url base-path)]
     {:tree (->> (reduce (fn [acc node] (assoc-in acc (string/split (:path node) #"/") node)) {} entries)
                 (mapv (fn [x] (->zip-resources workspace (io/as-url file-or-url) "" x))))
      :crc (into {} (map (juxt (fn [e] (str "/" (:path e))) :crc) entries))})))

(g/defnode ResourceNode
  (extern resource Resource (dynamic visible (g/constantly false))))

(defn base-name ^String [resource]
  (FilenameUtils/getBaseName (resource-name resource)))

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
    (let [^File f (fs/create-temp-file! "tmp" (format ".%s" (ext resource)))]
      (with-open [in (io/input-stream resource)]
        (io/copy in f))
      (.getAbsolutePath f))))

(defn style-classes [resource]
  (into #{"resource"}
        (keep not-empty)
        [(when (read-only? resource) "resource-read-only")
         (case (source-type resource)
           :file (some->> resource ext not-empty (str "resource-ext-"))
           :folder "resource-folder"
           nil)]))

(defn ext-style-classes [resource-ext]
  (assert (or (nil? resource-ext) (string? resource-ext)))
  (if-some [ext (not-empty resource-ext)]
    #{"resource" (str "resource-ext-" ext)}
    #{"resource"}))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (path r) (into-array String []))] (.matches matcher path))) resources)))
