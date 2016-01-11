(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.library :as library])

  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils IOUtils]
           [editor.resource FileResource]))

(defprotocol SelectionProvider
  (selection [this]))

(def build-dir "/build/default/")

(defn project-path [workspace]
  (io/as-file (g/node-value workspace :root)))

(defn build-path [workspace]
  (str (project-path workspace) build-dir))

(defrecord BuildResource [resource prefix]
  resource/Resource
  (children [this] nil)
  (resource-type [this] (resource/resource-type resource))
  (source-type [this] (resource/source-type resource))
  (read-only? [this] false)
  (path [this] (let [ext (:build-ext (resource/resource-type this) "unknown")
                     suffix (format "%x" (resource/resource-hash this))]
                 (if-let [path (resource/path resource)]
                   (str (FilenameUtils/removeExtension path) "." ext)
                   (str prefix "_generated_" suffix "." ext))))
  (abs-path [this] (.getAbsolutePath (File. (str (build-path (resource/workspace this)) (resource/path this)))))
  (proj-path [this] (str "/" (resource/path this)))
  ; TODO
  (url [this] nil)
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (File. (resource/abs-path this)) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (let [file (File. (resource/abs-path this))] (io/make-output-stream file opts)))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defn make-build-resource
  ([resource]
    (make-build-resource resource nil))
  ([resource prefix]
    (BuildResource. resource prefix)))

(g/defnk produce-resource-tree [_node-id root resource-snapshot]
  (FileResource. _node-id (io/as-file root) (:resources resource-snapshot)))

(g/defnk produce-resource-list [resource-tree]
  (resource/resource-seq resource-tree))

(g/defnk produce-resource-map [resource-list]
  (into {} (map #(do [(resource/proj-path %) %]) resource-list)))

(defn get-view-type [workspace id]
  (get (g/node-value workspace :view-types) id))

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
  (get (g/node-value workspace :resource-types) ext))

(defn get-resource-types
  ([workspace]
    (map second (g/node-value workspace :resource-types)))
  ([workspace tag]
    (filter #(contains? (:tags %) tag) (map second (g/node-value workspace :resource-types)))))

(defn template [resource-type]
  (when-let [template-path (or (:template resource-type) (str "templates/template." (:ext resource-type)))]
    (when-let [resource (io/resource template-path)]
      (with-open [f (io/reader resource)]
        (slurp f)))))

(def default-icons {:file "icons/32/Icons_29-AT-Unkown.png" :folder "icons/32/Icons_01-Folder-closed.png"})

(defn resource-icon [resource]
  (and resource (or (:icon (resource/resource-type resource)) (get default-icons (resource/source-type resource)))))

(defn file-resource [workspace path]
  (FileResource. workspace (File. (str (g/node-value workspace :root) path)) []))

(defn find-resource [workspace proj-path]
  (get (g/node-value workspace :resource-map) proj-path))

(defn resolve-workspace-resource [workspace path]
  (or
   (find-resource workspace path)
   (file-resource workspace path)))

(defn- absolute-path [^String path]
  (.startsWith path "/"))

(defn to-absolute-path
  ([rel-path] (to-absolute-path "" rel-path))
  ([base rel-path]
   (if (absolute-path rel-path)
     rel-path
     (str base "/" rel-path))))

(defn resolve-resource [base-resource path]
  (when-not (empty? path)
    (let [path (if (absolute-path path)
                 path
                 (to-absolute-path (str (.getParent (File. (resource/proj-path base-resource)))) path))]
      (when-let [workspace (:workspace base-resource)]
        (resolve-workspace-resource workspace path)))))

(defn set-project-dependencies! [workspace library-url-string]
  (let [library-urls (library/parse-library-urls (str library-url-string))]
    (g/set-property! workspace :dependencies library-urls)
    library-urls))

(defn update-dependencies! [workspace]
  (library/update-libraries! (project-path workspace)
                             (g/node-value workspace :dependencies)))

(defn resource-sync!
  ([workspace]
   (resource-sync! workspace true []))
  ([workspace notify-listeners?]
   (resource-sync! workspace notify-listeners? []))
  ([workspace notify-listeners? moved-files]
   (let [project-path (project-path workspace)
         moved-paths (mapv (fn [[src trg]] [(resource/file->proj-path project-path src)
                                            (resource/file->proj-path project-path trg)]) moved-files)
         old-snapshot (g/node-value workspace :resource-snapshot)
         old-map (resource-watch/make-resource-map old-snapshot)
         new-snapshot (resource-watch/make-snapshot workspace
                                                    project-path
                                                    (g/node-value workspace :dependencies))
         new-map (resource-watch/make-resource-map new-snapshot)
         changes (resource-watch/diff old-snapshot new-snapshot)]
     (when (or (not (resource-watch/empty-diff? changes)) (seq moved-paths))
       (g/set-property! workspace :resource-snapshot new-snapshot)
       (when notify-listeners?
         (let [changes (into {} (map (fn [[type resources]] [type (filter (comp some? resource/resource-type) resources)]) changes)) ; skip unknown resources
               move-srcs (set (map first moved-paths))
               move-trgs (set (map second moved-paths))
               ;; the new snapshot will show the source of the move as
               ;; * :removed, if no previously unloadable lib provides a resource with that path
               ;; * :changed, if a previously unloadable lib provides a resource with that path
               ;; We don't want to treat the path as removed:
               non-moved-removed (remove (comp move-srcs resource/proj-path) (:removed changes))
               ;; Neither do we want to treat it as changed...
               non-moved-changed (remove (comp move-srcs resource/proj-path) (:changed changes))
               ;; ... instead we want to add a new node in its place (since the old node for the path will be re-pointed to the move target resource)
               added-with-changed-move-srcs (concat (filter (comp move-srcs resource/proj-path) (:changed changes))
                                                    ;; Also, since we reuse the source node for the target resource, we remove the target from the added list
                                                    (remove (comp move-trgs resource/proj-path) (:added changes)))
               move-adjusted-changes {:removed non-moved-removed
                                      :added added-with-changed-move-srcs
                                      :changed non-moved-changed
                                      :moved (mapv (fn [[src trg]] [(old-map src) (new-map trg)]) moved-paths)}]
           (doseq [listener @(g/node-value workspace :resource-listeners)]
             (resource/handle-changes listener move-adjusted-changes)))))
     changes)))

(defn add-resource-listener! [workspace listener]
  (swap! (g/node-value workspace :resource-listeners) conj listener))

(g/defnode Workspace
  (property root g/Str)
  (property dependencies g/Any) ; actually vector of URLs
  (property opened-files g/Any (default (atom #{})))
  (property resource-snapshot g/Any)
  (property resource-listeners g/Any (default (atom [])))
  (property view-types g/Any)
  (property resource-types g/Any)

  (output resource-tree FileResource :cached produce-resource-tree)
  (output resource-list g/Any :cached produce-resource-list)
  (output resource-map g/Any :cached produce-resource-map)
  (output resource-types g/Any :cached (g/fnk [resource-types] resource-types)))

(defn make-workspace [graph project-path]
  (g/make-node! graph Workspace
                :root project-path
                :resource-snapshot (resource-watch/empty-snapshot)
                :view-types {:default {:id :default}}
                :resource-listeners (atom [])))

(defn register-view-type [workspace & {:keys [id make-view-fn make-preview-fn]}]
  (let [view-type {:id id :make-view-fn make-view-fn :make-preview-fn make-preview-fn}]
     (g/update-property workspace :view-types assoc (:id view-type) view-type)))
