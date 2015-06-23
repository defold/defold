(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils IOUtils]))

(defprotocol SelectionProvider
  (selection [this]))

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

(def wrap-stream)

(defn- split-ext [^String n]
  (let [i (.lastIndexOf n ".")]
    (if (pos? i)
      [(subs n 0 i) (subs n (inc i))]
      [n nil])))

(defn- relative-path [^File f1 ^File f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

(defrecord FileResource [workspace ^File file children]
  Resource
  (resource-type [this] (get (:resource-types workspace) (second (split-ext (.getPath file)))))
  (source-type [this] (if (.isFile file) :file :folder))
  (read-only? [this] (not (.canWrite file)))
  (path [this] (if (= "" (.getName file)) "" (relative-path (File. ^String (:root workspace)) file)))
  (abs-path [this] (.getAbsolutePath  file))
  (proj-path [this] (if (= "" (.getName file)) "" (str "/" (path this))))
  (url [this] (relative-path (File. ^String (:root workspace)) file))
  (resource-name [this] (.getName file))
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream file opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (wrap-stream workspace (io/make-output-stream file opts) file))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

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

(defmethod print-method MemoryResource [memory-resource ^java.io.Writer w]
  (.write w (format "MemoryResource{:workspace %s :data %s}" (:workspace memory-resource) (:data memory-resource))))

(defn make-memory-resource [workspace resource-type data]
  (MemoryResource. workspace resource-type data))

(defrecord ZipResource [workspace name path data children]
  Resource
  (resource-type [this] (get (:resource-types workspace) (second (split-ext name))))
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

(defn- make-zip-tree [workspace file-name]
  (let [entries (load-zip file-name)]
    (->> (reduce (fn [acc node] (assoc-in acc (string/split (:path node) #"/") node)) {} entries)
      (mapv (fn [x] (->zip-resources workspace "" x))))))

(def build-dir "/build/default/")

(defn build-path [workspace]
  (str (:root workspace) build-dir))

(defrecord BuildResource [resource prefix]
  Resource
  (resource-type [this] (resource-type resource))
  (source-type [this] (source-type resource))
  (read-only? [this] false)
  (path [this] (let [ext (:build-ext (resource-type this) "unknown")
                     suffix (format "%x" (resource-hash this))]
                 (if-let [path (path resource)]
                   (str (first (split-ext path)) "." ext)
                   (str prefix "_generated_" suffix "." ext))))
  (abs-path [this] (.getAbsolutePath (File. ^String (str (build-path (workspace this)) (path this)))))
  (proj-path [this] (str "/" (path this)))
  ; TODO
  (url [this] nil)
  (resource-name [this] (resource-name resource))
  (workspace [this] (workspace resource))
  (resource-hash [this] (resource-hash resource))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (File. ^String (abs-path this)) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (let [file (File. ^String (abs-path this))] (wrap-stream (workspace this) (io/make-output-stream file opts) file)))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defn make-build-resource
  ([resource]
    (make-build-resource resource nil))
  ([resource prefix]
    (BuildResource. resource prefix)))

(defn- create-resource-tree [workspace ^File file filter-fn]
  (let [children (if (.isFile file) [] (mapv #(create-resource-tree workspace % filter-fn) (filter filter-fn (.listFiles file))))]
    (FileResource. workspace file children)))

(g/defnk produce-resource-tree [self ^String root]
  (let [tree (create-resource-tree self (File. root) (fn [^File f]
                                                       (let [name (.getName f)]
                                                         (not (or (= name "build") (= (subs name 0 1) "."))))))]
    (update-in tree [:children] concat (make-zip-tree nil (io/resource "builtins.zip")))))

(g/defnk produce-resource-list [self resource-tree]
  (tree-seq #(= :folder (source-type %)) :children resource-tree))

(defn get-view-type [workspace id]
  (get (:view-types workspace) id))

(defn register-resource-type [workspace & {:keys [ext build-ext node-type load-fn icon view-types view-opts tags template]}]
  (let [resource-type {:ext ext
                       :build-ext (if (nil? build-ext) (str ext "c") build-ext)
                       :node-type node-type
                       :load-fn load-fn
                       :icon icon
                       :view-types (map (partial get-view-type workspace) view-types)
                       :view-opts view-opts
                       :tags tags
                       :template template}
        resource-types (if (string? ext)
                         [(assoc resource-type :ext ext)]
                         (map (fn [ext] (assoc resource-type :ext ext)) ext))]
    (for [resource-type resource-types]
      (g/update-property workspace :resource-types assoc (:ext resource-type) resource-type))))

(defn get-resource-type [workspace ext]
  (get (:resource-types workspace) ext))

(defn get-resource-types
  ([workspace]
    (map second (:resource-types workspace)))
  ([workspace tag]
    (filter #(contains? (:tags %) tag) (map second (:resource-types workspace)))))

(defn template [resource-type]
  (when-let [template-path (:template resource-type)]
    (with-open [f (io/reader (io/resource template-path))]
      (slurp f))))

(def default-icons {:file "icons/page_white.png" :folder "icons/folder.png"})

(defn resource-icon [resource]
  (and resource (or (:icon (resource-type resource)) (get default-icons (source-type resource)))))

(defn file-resource [workspace path]
  (FileResource. workspace (File. (str (:root workspace) path)) []))

(defn resolve-resource [base-resource path]
  (let [workspace (:workspace base-resource)
        full-path (if (empty? path) path (str (:root workspace) path))]
    ; TODO handle relative paths
    (FileResource. workspace (File. full-path) [])))

(g/defnode Workspace
  (property root g/Str)
  (property opened-files g/Any (default (atom #{})))
  (property view-types g/Any)
  (property resource-types g/Any)

  (output resource-tree FileResource :cached produce-resource-tree)
  (output resource-list g/Any :cached produce-resource-list)
  (output resource-types g/Any :cached (g/fnk [resource-types] resource-types)))

(defn make-workspace [graph project-path]
  (g/make-node! graph Workspace :root project-path :view-types {:default {:id :default}}))

(defn- wrap-stream [workspace stream file]
  (swap! (:opened-files workspace)
        (fn [rs]
          (when (get rs file)
            (throw (Exception. (format "File %s already opened" file))))
          (conj rs file)))
  (proxy [FilterOutputStream] [stream]
   (close []
     (let [^FilterOutputStream this this]
       (swap! (:opened-files workspace) disj file)
          (proxy-super close)))))

(defn register-view-type [workspace & {:keys [id make-view-fn make-preview-fn]}]
  (let [view-type {:id id :make-view-fn make-view-fn :make-preview-fn make-preview-fn}]
     (g/update-property workspace :view-types assoc (:id view-type) view-type)))
