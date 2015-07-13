(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [potemkin.namespaces :as namespaces]
            [editor.resource :as resource]
            [editor.fs-watch :as fs-watch])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils IOUtils]
           [editor.resource FileResource MemoryResource ZipResource]))

(namespaces/import-vars [editor.resource Resource ResourceListener resource-type source-type read-only? path abs-path proj-path url resource-name workspace resource-hash handle-changes make-zip-tree])

(defprotocol SelectionProvider
  (selection [this]))

(defn make-memory-resource [workspace resource-type data]
  (MemoryResource. workspace resource-type data))

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
                   (str (FilenameUtils/removeExtension path) "." ext)
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
  (io/make-output-stream [this opts] (let [file (File. ^String (abs-path this))] (io/make-output-stream file opts)))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defn make-build-resource
  ([resource]
    (make-build-resource resource nil))
  ([resource prefix]
    (BuildResource. resource prefix)))

(defn- resource-filter [^File f]
  (let [name (.getName f)]
    (not (or (= name "build") (= (subs name 0 1) ".")))))

(defn- create-resource-tree [workspace ^File file]
  (let [children (if (.isFile file) [] (mapv #(create-resource-tree workspace %) (filter resource-filter (.listFiles file))))]
    (FileResource. workspace file children)))

(g/defnk produce-resource-tree [self ^String root]
  (let [tree (create-resource-tree self (File. root))]
    (update-in tree [:children] concat (make-zip-tree self (io/resource "builtins.zip")))))

(g/defnk produce-resource-list [self resource-tree]
  (tree-seq #(= :folder (source-type %)) :children resource-tree))

(g/defnk produce-resource-map [self resource-list]
  (into {} (map #(do [(proj-path %) %]) resource-list)))

(defn get-view-type [workspace id]
  (get (:view-types workspace) id))

(defn register-resource-type [workspace & {:keys [ext build-ext node-type load-fn icon view-types view-opts tags template label]}]
  (let [resource-type {:ext ext
                       :build-ext (if (nil? build-ext) (str ext "c") build-ext)
                       :node-type node-type
                       :load-fn load-fn
                       :icon icon
                       :view-types (map (partial get-view-type workspace) view-types)
                       :view-opts view-opts
                       :tags tags
                       :template template
                       :label label}
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

(def default-icons {:file "icons/page_white.png" :folder "icons/16/Icons_01-Folder-closed.png"})

(defn resource-icon [resource]
  (and resource (or (:icon (resource-type resource)) (get default-icons (source-type resource)))))

(defn file-resource [workspace path]
  (FileResource. workspace (File. (str (:root workspace) path)) []))

(defn find-resource [workspace path]
  (let [proj-path path]
    (get (g/node-value workspace :resource-map) proj-path)))

(defn resolve-resource [base-resource path]
    ; TODO handle relative paths
  (find-resource (:workspace base-resource) path))

(defn fs-sync
  ([workspace]
    (fs-sync workspace true))
  ([workspace notify-listeners?]
  (let [watcher-atom (:fs-watcher workspace)
        watcher (swap! (:fs-watcher workspace) fs-watch/watch)
        changes (into {} (map (fn [[type files]] [type (map #(FileResource. workspace % []) files)]) (:changes watcher)))]
    (when (or (not (empty? (:added changes))) (not (empty? (:removed changes))))
      ; TODO - bug in graph when invalidating internal dependencies, need to be explicit for every output
      (let [ws-id (g/node-id workspace)]
        (g/invalidate! (mapv #(do [ws-id %]) [:resource-tree :resource-list :resource-map]))))
    (when notify-listeners?
      (doseq [listener @(:resource-listeners workspace)]
        (handle-changes listener changes))))))

(defn add-resource-listener! [workspace listener]
  (swap! (:resource-listeners workspace) conj listener))

(g/defnode Workspace
  (property root g/Str)
  (property opened-files g/Any (default (atom #{})))
  (property fs-watcher g/Any)
  (property resource-listeners g/Any (default (atom [])))
  (property view-types g/Any)
  (property resource-types g/Any)

  (output resource-tree FileResource :cached produce-resource-tree)
  (output resource-list g/Any :cached produce-resource-list)
  (output resource-map g/Any :cached produce-resource-map)
  (output resource-types g/Any :cached (g/fnk [resource-types] resource-types)))

(defn make-workspace [graph project-path]
  (g/make-node! graph Workspace :root project-path :view-types {:default {:id :default}}
                :fs-watcher (atom (fs-watch/make-watcher project-path resource-filter))
                :resource-listeners (atom [])))

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
