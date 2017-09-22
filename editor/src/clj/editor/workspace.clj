(ns editor.workspace
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.progress :as progress]
            [editor.resource-watch :as resource-watch]
            [editor.library :as library]
            [editor.graph-util :as gu]
            [editor.url :as url]
            [service.log :as log])
  (:import [java.io ByteArrayOutputStream File FilterOutputStream]
           [java.net URL]
           [java.util.zip ZipEntry ZipInputStream]
           [org.apache.commons.io FilenameUtils]
           [editor.resource FileResource]))

(set! *warn-on-reflection* true)

(def build-dir "/build/default/")

(defn project-path ^File [workspace]
  (io/as-file (g/node-value workspace :root)))

(defn build-path [workspace]
  (str (project-path workspace) build-dir))

(defrecord BuildResource [resource prefix]
  resource/Resource
  (children [this] nil)
  (ext [this] (:build-ext (resource/resource-type this) "unknown"))
  (resource-type [this] (resource/resource-type resource))
  (source-type [this] (resource/source-type resource))
  (read-only? [this] false)
  (path [this] (let [ext (resource/ext this)
                     suffix (format "%x" (resource/resource-hash this))]
                 (if-let [path (resource/path resource)]
                   (str (FilenameUtils/removeExtension path) "." ext)
                   (str prefix "_generated_" suffix "." ext))))
  (abs-path [this] (.getAbsolutePath (File. (str (build-path (resource/workspace this)) (resource/path this)))))
  (proj-path [this] (str "/" (resource/path this)))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))
  (openable? [this] false)

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (File. (resource/abs-path this)) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (let [file (File. (resource/abs-path this))] (io/make-output-stream file opts)))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  io/Coercions
  (io/as-file [this] (File. (resource/abs-path this))))

(defn make-build-resource
  ([resource]
    (make-build-resource resource nil))
  ([resource prefix]
   (BuildResource. resource prefix)))

(defn sort-resource-tree [{:keys [children] :as tree}]
  (let [sorted-children (sort-by (fn [r]
                                   [(not (resource/read-only? r))
                                    ({:folder 0 :file 1} (resource/source-type r))
                                    (when-let [node-name (resource/resource-name r)]
                                      (string/lower-case node-name))])
                                 (map sort-resource-tree children))]
    (assoc tree :children (vec sorted-children))))

(g/defnk produce-resource-tree [_node-id root resource-snapshot]
  (sort-resource-tree
    (resource/make-file-resource _node-id root (io/as-file root) (:resources resource-snapshot))))

(g/defnk produce-resource-list [resource-tree]
  (resource/resource-seq resource-tree))

(g/defnk produce-resource-map [resource-list]
  (into {} (map #(do [(resource/proj-path %) %]) resource-list)))

(defn get-view-type [workspace id]
  (get (g/node-value workspace :view-types) id))

(defn register-resource-type [workspace & {:keys [textual? ext build-ext node-type load-fn read-fn write-fn icon view-types view-opts tags tag-opts template label]}]
  (let [resource-type {:textual? (true? textual?)
                       :ext ext
                       :build-ext (if (nil? build-ext) (str ext "c") build-ext)
                       :node-type node-type
                       :load-fn load-fn
                       :write-fn write-fn
                       :read-fn read-fn
                       :icon icon
                       :view-types (map (partial get-view-type workspace) view-types)
                       :view-opts view-opts
                       :tags tags
                       :tag-opts tag-opts
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

(defn- template-path [resource-type]
  (or (:template resource-type)
      (some->> resource-type :ext (str "templates/template."))))

(defn has-template? [resource-type]
  (some? (some-> resource-type template-path io/resource)))

(defn template [resource-type]
  (when-let [template-path (template-path resource-type)]
    (when-let [resource (io/resource template-path)]
      (with-open [f (io/reader resource)]
        (slurp f)))))

(defn resource-icon [resource]
  (when resource
    (if (and (resource/read-only? resource)
             (= (resource/path resource) (resource/resource-name resource)))
      "icons/32/Icons_03-Builtins.png"
      (condp = (resource/source-type resource)
        :file
        (or (:icon (resource/resource-type resource)) "icons/32/Icons_29-AT-Unknown.png")
        :folder
        "icons/32/Icons_01-Folder-closed.png"))))

(defn file-resource [workspace path-or-file]
  (let [root (g/node-value workspace :root)
        f (if (instance? File path-or-file)
            path-or-file
            (File. (str root path-or-file)))]
    (resource/make-file-resource workspace root f [])))

(defn find-resource [workspace proj-path]
  (get (g/node-value workspace :resource-map) proj-path))

(defn resolve-workspace-resource [workspace path]
  (when (and path (not-empty path))
    (or
      (find-resource workspace path)
      (file-resource workspace path))))

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

(defn set-project-dependencies! [workspace library-urls]
  (g/set-property! workspace :dependencies library-urls)
  library-urls)

(defn dependencies [workspace]
  (g/node-value workspace :dependencies))

(defn update-dependencies! [workspace render-progress! login-fn]
  (let [dependencies (g/node-value workspace :dependencies)
        hosts (into #{} (map url/strip-path) dependencies)]
    (when (and (seq dependencies)
               (every? url/reachable? hosts)
               (or (nil? login-fn)
                   (not-any? url/defold-hosted? dependencies)
                   (login-fn)))
      (library/update-libraries! (project-path workspace)
                                 dependencies
                                 library/default-http-resolver
                                 render-progress!)
      true)))

(defn missing-dependencies [workspace]
  (let [project-directory (project-path workspace)
        dependencies (g/node-value workspace :dependencies)]
    (into #{}
          (comp (remove :file)
                (map :url))
          (library/current-library-state project-directory dependencies))))

(defn invalidate-resources! [workspace paths]
  (g/update-property! workspace :resource-snapshot resource-watch/invalidate-resources paths))

(defn resource-sync!
  ([workspace]
   (resource-sync! workspace true []))
  ([workspace notify-listeners?]
   (resource-sync! workspace notify-listeners? []))
  ([workspace notify-listeners? moved-files]
   (resource-sync! workspace notify-listeners? moved-files progress/null-render-progress!))
  ([workspace notify-listeners? moved-files render-progress!]
   (let [project-path (project-path workspace)
         moved-proj-paths (keep (fn [[src tgt]]
                                  (let [src-path (resource/file->proj-path project-path src)
                                        tgt-path (resource/file->proj-path project-path tgt)]
                                    (assert (some? src-path) (str "project does not contain source " (pr-str src)))
                                    (assert (some? tgt-path) (str "project does not contain target " (pr-str tgt)))
                                    (when (not= src-path tgt-path)
                                      [src-path tgt-path])))
                                moved-files)
         old-snapshot (g/node-value workspace :resource-snapshot)
         old-map      (resource-watch/make-resource-map old-snapshot)
         _            (render-progress! (progress/make "Finding resources..."))
         new-snapshot (resource-watch/make-snapshot workspace
                                                    project-path
                                                    (g/node-value workspace :dependencies))
         new-map      (resource-watch/make-resource-map new-snapshot)
         changes      (resource-watch/diff old-snapshot new-snapshot)]
     (when (or (not (resource-watch/empty-diff? changes)) (seq moved-proj-paths))
       (g/set-property! workspace :resource-snapshot new-snapshot)
       (when notify-listeners?
         (let [changes (into {} (map (fn [[type resources]] [type (filter #(= :file (resource/source-type %)) resources)]) changes))
               move-source-paths (map first moved-proj-paths)
               move-target-paths (map second moved-proj-paths)
               chain-moved-paths (set/intersection (set move-source-paths) (set move-target-paths))
               merged-target-paths (set (map first (filter (fn [[k v]] (> v 1)) (frequencies move-target-paths))))
               moved (keep (fn [[source-path target-path]]
                             (when-not (or
                                         ;; resource sync currently can't handle chained moves, so refactoring is
                                         ;; temporarily disabled for those cases (no move pair)
                                         (chain-moved-paths source-path) (chain-moved-paths target-path)
                                         ;; also can't handle merged targets, multiple files with same name moved to same dir
                                         (merged-target-paths target-path))
                               (let [src-resource (old-map source-path)
                                     tgt-resource (new-map target-path)]
                                 ;; We used to (assert (some? src-resource)), but this could fail for instance if
                                 ;; * source-path refers to a .dotfile (like .DS_Store) that we ignore in resource/make-snapshot
                                 ;; * Some external process has created a file in a to-be-moved directory and we haven't run a resource-sync! before the move
                                 ;; We handle these cases by ignoring the move. Any .dotfiles will stay ignored, and any new files will pop up as :added
                                 ;;
                                 ;; We also used to (assert (some? tgt-resource)) but an arguably very unlikely case is that the target of the move is
                                 ;; deleted from disk after the move but before the snapshot.
                                 ;; We handle that by ignoring the move and effectively treating target as just :removed.
                                 ;; The source will be :removed or :changed (if a library snuck in).
                                 (cond
                                   (nil? src-resource)
                                   (do (log/warn :msg (str "can't find source of move " source-path)) nil)

                                   (nil? tgt-resource)
                                   (do (log/warn :msg (str "can't find target of move " target-path)) nil)

                                   (and (= :file (resource/source-type src-resource))
                                        (= :file (resource/source-type tgt-resource))) ; paranoia
                                   [src-resource tgt-resource]))))
                           moved-proj-paths)
               changes-with-moved (assoc changes :moved moved)]
           (assert (= (count (distinct (map (comp resource/proj-path first) moved)))
                      (count (distinct (map (comp resource/proj-path second) moved)))
                      (count moved))) ; no overlapping sources, dito targets
           (assert (= (count (distinct (concat (map (comp resource/proj-path first) moved)
                                               (map (comp resource/proj-path second) moved))))
                      (* 2 (count moved)))) ; no chained moves src->tgt->tgt2...
           (assert (empty? (set/intersection (set (map (comp resource/proj-path first) moved))
                                             (set (map resource/proj-path (:added changes)))))) ; no move-source is in :added
           (doseq [listener @(g/node-value workspace :resource-listeners)]
             (resource/handle-changes listener changes-with-moved render-progress!)))))
     changes)))

(defn fetch-libraries!
  [workspace library-urls render-fn login-fn]
  (set-project-dependencies! workspace library-urls)
  (when (update-dependencies! workspace render-fn login-fn)
    (resource-sync! workspace true [] render-fn)))

(defn add-resource-listener! [workspace listener]
  (swap! (g/node-value workspace :resource-listeners) conj listener))


(g/deftype UrlVec [URL])

(g/defnode Workspace
  (property root g/Str)
  (property dependencies UrlVec)
  (property opened-files g/Any (default (atom #{})))
  (property resource-snapshot g/Any)
  (property resource-listeners g/Any (default (atom [])))
  (property view-types g/Any)
  (property resource-types g/Any)
  (property library-snapshot-cache g/Any (default {}))

  (output resource-tree FileResource :cached produce-resource-tree)
  (output resource-list g/Any :cached produce-resource-list)
  (output resource-map g/Any :cached produce-resource-map))

(defn make-workspace [graph project-path]
  (g/make-node! graph Workspace
                :root project-path
                :resource-snapshot (resource-watch/empty-snapshot)
                :view-types {:default {:id :default}}
                :resource-listeners (atom [])))

(defn register-view-type [workspace & {:keys [id label make-view-fn make-preview-fn focus-fn]}]
  (let [view-type (merge {:id    id
                          :label label}
                         (when make-view-fn
                           {:make-view-fn make-view-fn})
                         (when make-preview-fn
                           {:make-preview-fn make-preview-fn})
                         (when focus-fn
                           {:focus-fn focus-fn}))]
     (g/update-property workspace :view-types assoc (:id view-type) view-type)))
