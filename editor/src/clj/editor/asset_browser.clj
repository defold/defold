;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.asset-browser
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.dialogs :as dialogs]
            [editor.disk-availability :as disk-availability]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.notifications :as notifications]
            [editor.handler :as handler]
            [editor.icons :as icons]
            [editor.prefs :as prefs]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :refer [pair]])
  (:import [com.defold.control TreeCell]
           [editor.resource FileResource]
           [java.io File]
           [java.nio.file Path Paths]
           [javafx.collections FXCollections ObservableList]
           [javafx.scene.input Clipboard ClipboardContent]
           [javafx.scene.input DragEvent MouseEvent TransferMode]
           [javafx.scene Node]
           [javafx.scene.control SelectionMode TreeItem TreeView]
           [javafx.scene.input MouseEvent]
           [javafx.stage Stage]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(declare tree-item)

(def ^:private empty-string-array (into-array String []))

(defn- ->path [s]
  (Paths/get s empty-string-array))

; TreeItem creator
(defn-  ^ObservableList list-children [parent]
  (let [children (:children parent)
        items (->> (:children parent)
                   (remove resource/internal?)
                   (map tree-item)
                   (into-array TreeItem))]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll ^"[Ljavafx.scene.control.TreeItem;" items)))))

; NOTE: Without caching stack-overflow... WHY?
(defn tree-item ^TreeItem [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (or (not= :folder (resource/source-type (.getValue ^TreeItem this)))
            (empty? (:children (.getValue ^TreeItem this)))))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(handler/register-menu! ::resource-menu
  [{:label "Open"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open}
   {:label "Open As"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open-as}
   {:label :separator}
   {:label "Copy Project Path"
    :command :copy-project-path}
   {:label "Copy Full Path"
    :command :copy-full-path}
   {:label "Copy Require Path"
    :command :copy-require-path}
   {:label :separator}
   {:label "Show in Desktop"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :show-in-desktop}
   {:label "Referencing Files..."
    :command :referencing-files}
   {:label "Dependencies..."
    :command :dependencies}
   {:label "Show Overrides"
    :command :show-overrides}
   {:label :separator}
   {:label "New"
    :command :new-file
    :expand? true
    :icon "icons/64/Icons_29-AT-Unknown.png"}
   {:label "New File"
    :command :new-file
    :user-data {:any-file true}
    :icon "icons/64/Icons_29-AT-Unknown.png"}
   {:label "New Folder"
    :command :new-folder
    :icon "icons/32/Icons_01-Folder-closed.png"}
   {:label :separator}
   {:label "Cut"
    :command :cut}
   {:label "Copy"
    :command :copy}
   {:label "Paste"
    :command :paste}
   {:label "Delete"
    :command :delete
    :icon "icons/32/Icons_M_06_trash.png"}
   {:label :separator}
   {:label "Rename..."
    :command :rename}
   {:label :separator
    :id ::context-menu-end}])

(def fixed-resource-paths #{"/" "/game.project"})

(defn deletable-resource? [x] (and (satisfies? resource/Resource x)
                                   (resource/editable? x)
                                   (not (resource/read-only? x))
                                   (not (fixed-resource-paths (resource/proj-path x)))))

(defn- roots [resources]
  (let [resources (into {} (map (fn [resource] [(->path (resource/proj-path resource)) resource]) resources))
        roots (loop [paths (keys resources)
                     roots []]
                (if-let [^Path path (first paths)]
                  (let [roots (if (empty? (filter (fn [^Path p] (.startsWith path p)) roots))
                                (conj roots path)
                                roots)]
                    (recur (rest paths) roots))
                  roots))]
    (mapv #(resources %) roots)))

(defn- temp-resource-file! [^File dir resource]
  (let [target (File. dir (resource/resource-name resource))]
    (if (= :file (resource/source-type resource))
      (with-open [in (io/input-stream resource)
                  out (io/output-stream target)]
        (io/copy in out))
      (do
        (fs/create-directories! target)
        (doseq [c (:children resource)]
          (temp-resource-file! target c))))
    target))

(defn- fileify-resources! [resources]
  (let [dnd-directory (fs/create-temp-directory! "asset-dnd")]
    (mapv (fn [r]
            (if (resource/file-resource? r)
              (io/file r)
              (temp-resource-file! dnd-directory r)))
          resources)))

(defn delete [resources]
  (when (not (empty? resources))
    (let [workspace (resource/workspace (first resources))]
      (doseq [resource resources]
        (let [f (File. (resource/abs-path resource))]
          (fs/delete! f {:fail :silently})))
      (workspace/resource-sync! workspace))))

(defn- copy [files]
  (let [cb (Clipboard/getSystemClipboard)
        content (ClipboardContent.)]
    (.putFiles content files)
    (.setContent cb content)))

(handler/defhandler :copy :asset-browser
  (enabled? [selection] (not (empty? selection)))
  (run [selection]
       (copy (-> selection roots fileify-resources!))))

(defn- select-resource!
  ([asset-browser resource]
   (select-resource! asset-browser resource nil))
  ([asset-browser resource {:keys [scroll?]
                            :or   {scroll? false}
                            :as opts}]
   ;; This is a hack!
   ;; The reason is that the FileResource 'next' fetched prior to the deletion
   ;; may have changed after the deletion, due to the 'children' field in the record.
   ;; This is why we can't use ui/select! directly, but do our own implementation based on path.
   (let [^TreeView tree-view (g/node-value asset-browser :tree-view)
         tree-items (ui/tree-item-seq (.getRoot tree-view))
         path (resource/resource->proj-path resource)]
     (when-let [tree-item (some (fn [^TreeItem tree-item] (and (= path (resource/resource->proj-path (.getValue tree-item))) tree-item)) tree-items)]
       (doto (.getSelectionModel tree-view)
         (.clearSelection)
         (.select tree-item))
       (when scroll?
         (ui/scroll-to-item! tree-view tree-item))))))

(defn delete? [resources]
  (and (disk-availability/available?)
       (seq resources)
       (every? deletable-resource? resources)))

(handler/defhandler :cut :asset-browser
  (enabled? [selection] (delete? selection))
  (run [selection selection-provider asset-browser]
    (let [next (-> (handler/succeeding-selection selection-provider)
                   (handler/adapt-single resource/Resource))
          cut-files-directory (fs/create-temp-directory! "asset-cut")]
      (copy (mapv #(temp-resource-file! cut-files-directory %) (roots selection)))
      (delete selection)
      (when next
        (select-resource! asset-browser next)))))

(defn- unique
  [^File original exists-fn name-fn]
  (let [original-basename (FilenameUtils/getBaseName (.getAbsolutePath original))]
    (loop [^File f original]
      (if (exists-fn f)
        (let [path (.getAbsolutePath f)
              ext (FilenameUtils/getExtension path)
              new-path (str (FilenameUtils/getFullPath path)
                            (name-fn original-basename (FilenameUtils/getBaseName path))
                            (when (seq ext) (str "." ext)))]
          (recur (File. new-path)))
        f))))

(defn- ensure-unique-dest-files
  [name-fn src-dest-pairs]
  (loop [[[src dest :as pair] & rest] src-dest-pairs
         new-names #{}
         ret []]
    (if pair
      (let [new-dest (unique dest #(or (.exists ^File %) (new-names %)) name-fn)]
        (recur rest (conj new-names new-dest) (conj ret [src new-dest])))
      ret)))

(defn- numbering-name-fn
  [original-basename basename]
  (let [suffix (string/replace basename original-basename "")]
    (if (.isEmpty suffix)
      (str original-basename "1")
      (str original-basename (inc (bigint suffix))))))

(defmulti resolve-conflicts (fn [strategy src-dest-pairs] strategy))

(defmethod resolve-conflicts :overwrite
  [_ src-dest-pairs]
  src-dest-pairs)

(defmethod resolve-conflicts :rename
  [_ src-dest-pairs]
  (ensure-unique-dest-files numbering-name-fn src-dest-pairs))

(defn- resolve-any-conflicts
  [src-dest-pairs]
  (let [files-by-existence (group-by (fn [[src ^File dest]] (.exists dest)) src-dest-pairs)
        conflicts (get files-by-existence true)
        non-conflicts (get files-by-existence false [])]
    (if (seq conflicts)
      (when-let [strategy (dialogs/make-resolve-file-conflicts-dialog conflicts)]
        (into non-conflicts (resolve-conflicts strategy conflicts)))
      non-conflicts)))

(defn- select-files! [workspace tree-view files]
  (let [selected-paths (mapv (partial resource/file->proj-path (workspace/project-path workspace)) files)]
    (ui/user-data! tree-view ::pending-selection selected-paths)))

(defn- reserved-project-file [^File project-path ^File f]
  (resource-watch/reserved-proj-path? project-path (resource/file->proj-path project-path f)))

(defn- illegal-copy-move-pairs [^File project-path prospect-pairs]
  (seq (filter (comp (partial reserved-project-file project-path) second) prospect-pairs)))

(defn allow-resource-move?
  "Returns true if it is legal to move all the supplied source files
  to the specified target resource. Disallows moves that would place a
  parent below one of its own children, moves to a readonly
  destination, moves to the same path the file already resides in, and
  moves to reserved directories."
  [tgt-resource src-files]
  (and (disk-availability/available?)
       (resource/editable? tgt-resource)
       (not (resource/read-only? tgt-resource))
       (let [^Path tgt-path (-> tgt-resource resource/abs-path File. fs/to-folder .getAbsolutePath ->path)
             src-paths (map (fn [^File f] (-> f .getAbsolutePath ->path))
                            src-files)
             descendant (some (fn [^Path p] (or (.equals tgt-path (.getParent p))
                                                (.startsWith tgt-path p)))
                              src-paths)
             ;; Below is a bit of a hack to only perform the reserved paths check if the target
             ;; is the project root.
             possibly-reserved-tgt-files (when (= (resource/proj-path tgt-resource) "/")
                                           (map #(.toFile (.resolve tgt-path (.getName ^File %))) src-files))
             project-path (workspace/project-path (resource/workspace tgt-resource))]
         (and (nil? descendant)
              (nil? (some (partial reserved-project-file project-path) possibly-reserved-tgt-files))))))

(defn paste? [files-on-clipboard? target-resources]
  (and files-on-clipboard?
       (disk-availability/available?)
       (= 1 (count target-resources))
       (let [target-resource (first target-resources)]
         (and (resource/editable? target-resource)
              (not (resource/read-only? target-resource))))))

(defn paste! [workspace target-resource src-files select-files!]
  (let [^File tgt-dir (reduce (fn [^File tgt ^File src]
                                (if (= tgt src)
                                  (.getParentFile ^File tgt)
                                  tgt))
                              (fs/to-folder (File. (resource/abs-path target-resource))) src-files)
        prospect-pairs (map (fn [^File f] [f (File. tgt-dir (FilenameUtils/getName (.toString f)))]) src-files)
        project-path (workspace/project-path workspace)]
    (if-let [illegal (illegal-copy-move-pairs project-path prospect-pairs)]
      (dialogs/make-info-dialog
        {:title "Cannot Paste"
         :icon :icon/triangle-error
         :header "There are reserved target directories"
         :content (str "Following target directories are reserved:\n"
                       (string/join "\n" (map (comp (partial resource/file->proj-path project-path) second) illegal)))})
      (let [pairs (ensure-unique-dest-files (fn [_ basename] (str basename "_copy")) prospect-pairs)]
        (doseq [[^File src-file ^File tgt-file] pairs]
          (fs/copy! src-file tgt-file {:target :merge}))
        (select-files! (mapv second pairs))
        (workspace/resource-sync! workspace)))))

(handler/defhandler :paste :asset-browser
  (enabled? [selection] (paste? (.hasFiles (Clipboard/getSystemClipboard)) selection))
  (run [selection workspace asset-browser]
       (let [tree-view (g/node-value asset-browser :tree-view)
             resource (first selection)
             src-files (.getFiles (Clipboard/getSystemClipboard))
             dest-path (resource/abs-path resource)]
         (if-let [conflicting-file (some #(when (string/starts-with? dest-path (.getPath %)) %) src-files)]
           (let [res-proj-path (resource/proj-path resource)
                 dest-proj-path (resource/file->proj-path (workspace/project-path workspace) conflicting-file)]
             (notifications/show!
               (workspace/notifications workspace)
               {:type :error
                :id ::asset-circular-paste
                :text (if (= res-proj-path dest-proj-path)
                        (str "Cannot paste folder '" dest-proj-path "' into itself")
                        (str "Cannot paste folder '" dest-proj-path "' into its subfolder '" res-proj-path "'"))}))
           (paste! workspace resource src-files (partial select-files! workspace tree-view))))))

(defn- moved-files
  [^File src-file ^File dest-file files]
  (let [src-path (.toPath src-file)
        dest-path (.toPath dest-file)]
    (mapv (fn [^File f]
            ;; (.relativize "foo" "foo") == "" so a plain file rename foo.clj -> bar.clj call of
            ;; of (moved-files "foo.clj" "bar.clj" ["foo.clj"]) will (.resolve "bar.clj" "") == "bar.clj"
            ;; just as we want.
            (let [dest-file (.toFile (.resolve dest-path (.relativize src-path (.toPath f))))]
              [f dest-file]))
          files)))

(defn rename? [resources]
  (and (disk-availability/available?)
       (pos? (count resources))
       (every? (fn [resource]
                 (and (resource/editable? resource)
                      (not (resource/read-only? resource))
                      (not (fixed-resource-paths (resource/resource->proj-path resource)))))
               resources)
       (case (into #{} (map resource/source-type) resources)
         #{:folder} (= 1 (count resources))
         #{:file} (and
                    (= 1 (count (into #{} (map resource/base-name) resources)))
                    (= (count resources) (count (into #{} (map resource/ext) resources))))
         false)))

(defn rename [resources new-base-name]
  {:pre [(string? new-base-name) (rename? resources)]}
  (let [workspace (resource/workspace (first resources))
        project-directory-file (workspace/project-path workspace)
        dir (= :folder (resource/source-type (first resources)))
        rename-pairs (mapv
                       (fn [resource]
                         (let [resource-file (io/file resource)
                               parent (.getParent resource-file)
                               ext (resource/ext resource)]
                           (pair resource-file
                                 (io/file parent (cond-> new-base-name
                                                         (and (not dir) (seq ext))
                                                         (str "." ext))))))
                       resources)]
    (when-not (some #(resource-watch/reserved-proj-path?
                       project-directory-file
                       (resource/file->proj-path project-directory-file (val %)))
                    rename-pairs)
      ;; plain case change causes irrelevant conflict on case-insensitive file systems
      ;; fs/move! handles this, no need to resolve
      (let [{case-changes true possible-conflicts false}
            (group-by #(fs/same-file? (key %) (val %)) rename-pairs)]
        (when-let [resolved-conflicts (resolve-any-conflicts possible-conflicts)]
          (let [resolved-rename-pairs (into resolved-conflicts case-changes)]
            (when (seq resolved-rename-pairs)
              (workspace/resource-sync!
                workspace
                (into []
                      (mapcat (fn [[src-file dst-file]]
                                (let [src-files (vec (file-seq src-file))]
                                  (fs/move! src-file dst-file)
                                  (moved-files src-file dst-file src-files))))
                      resolved-rename-pairs)))))))))

(defn validate-new-resource-name [^File project-directory-file parent-path new-name]
  (let [prospect-path (str parent-path "/" new-name)]
    (when (resource-watch/reserved-proj-path? project-directory-file prospect-path)
      (format "The name %s is reserved" new-name))))

(handler/defhandler :rename :asset-browser
  (enabled? [selection] (rename? selection))
  (run [selection workspace]
    (let [first-resource (first selection)
          dir (= :folder (resource/source-type first-resource))
          name (if dir
                 (resource/resource-name first-resource)
                 (resource/base-name first-resource))
          extensions (if dir [""] (mapv resource/ext selection))
          parent-paths (mapv (comp resource/parent-proj-path resource/proj-path) selection)
          project-directory-file (workspace/project-path workspace)]
      (when-let [new-name (dialogs/make-rename-dialog
                            name
                            :title (cond
                                     dir "Rename Folder"
                                     (= 1 (count selection)) "Rename File"
                                     :else "Rename Files")
                            :label (if dir "New Folder Name" "New File Name")
                            :extensions extensions
                            :validate (fn [file-name]
                                        (some #(validate-new-resource-name project-directory-file % file-name) parent-paths)))]
        (rename selection new-name)))))

(handler/defhandler :delete :asset-browser
  (enabled? [selection] (delete? selection))
  (run [selection asset-browser selection-provider]
    (let [next (-> (handler/succeeding-selection selection-provider)
                   (handler/adapt-single resource/Resource))]
      (when (if (= 1 (count selection))
              (dialogs/make-confirmation-dialog
                {:title "Delete File?"
                 :icon :icon/circle-question
                 :header (format "Are you sure you want to delete %s?"
                                 (resource/resource-name (first selection)))
                 :buttons [{:text "Cancel"
                            :cancel-button true
                            :default-button true
                            :result false}
                           {:text "Delete"
                            :variant :danger
                            :result true}]})
              (dialogs/make-info-dialog
                {:title "Delete Files?"
                 :icon :icon/circle-question
                 :header "Are you sure you want to delete these files?"
                 :content {:text (str "You are about to delete:\n"
                                      (->> selection
                                           (map #(str "\u00A0\u00A0\u2022\u00A0"
                                                      (resource/resource-name %)))
                                           (string/join "\n")))}
                 :buttons [{:text "Cancel"
                            :cancel-button true
                            :default-button true
                            :result false}
                           {:text "Delete"
                            :variant :danger
                            :result true}]}))
        (when (and (delete selection) next)
          (select-resource! asset-browser next))))))

(defn replace-template-name
  ^String [^String template ^String name]
  (let [escaped-name (protobuf/escape-string name)]
    (string/replace template "{{NAME}}" escaped-name)))

(defn- create-template-file! [^String template ^File new-file]
  (let [base-name (FilenameUtils/getBaseName (.getPath new-file))
        contents (replace-template-name template base-name)]
    (spit new-file contents)))

(handler/defhandler :new-file :global
  (label [user-data] (if-not user-data
                       "New..."
                       (let [rt (:resource-type user-data)]
                         (or (:label rt) (:ext rt)))))
  (active? [selection selection-context] (or (= :global selection-context) (and (= :asset-browser selection-context)
                                                                                (= (count selection) 1)
                                                                                (not= nil (some-> (handler/adapt-single selection resource/Resource)
                                                                                            resource/abs-path)))))
  (enabled? [] (disk-availability/available?))
  (run [selection user-data asset-browser app-view prefs workspace project]
    (let [project-path (workspace/project-path workspace)
          base-folder (-> (or (some-> (handler/adapt-every selection resource/Resource)
                                first
                                resource/abs-path
                                (File.))
                              project-path)
                          fs/to-folder)
          rt (:resource-type user-data)
          any-file (:any-file user-data false)]
      (when-let [desired-file (dialogs/make-new-file-dialog
                                project-path
                                base-folder
                                (when-not any-file
                                  (or (:label rt) (:ext rt)))
                                (:ext rt))]
        (when-let [[[_ ^File new-file]] (resolve-any-conflicts [[nil desired-file]])]
          (let [rt (if (and any-file (not rt))
                     (when-let [ext (second (re-find #"\.(.+)$" (.getName new-file)))]
                       (workspace/get-resource-type workspace ext))
                     rt)
                template (or (workspace/template workspace rt) "")]
            (create-template-file! template new-file))
          (workspace/resource-sync! workspace)
          (let [resource-map (g/node-value workspace :resource-map)
                new-resource-path (resource/file->proj-path project-path new-file)
                resource (resource-map new-resource-path)]
            (app-view/open-resource app-view prefs workspace project resource)
            (select-resource! asset-browser resource))))))
  (options [workspace selection user-data]
    (when (not user-data)
      (sort-by (comp string/lower-case :label)
               (into [{:label "File"
                       :icon "icons/64/Icons_29-AT-Unknown.png"
                       :command :new-file
                       :user-data {:any-file true}}]
                     (keep (fn [[_ext resource-type]]
                             (when (workspace/has-template? workspace resource-type)
                               {:label (or (:label resource-type) (:ext resource-type))
                                :icon (:icon resource-type)
                                :style (resource/type-style-classes resource-type)
                                :command :new-file
                                :user-data {:resource-type resource-type}})))
                     (workspace/get-resource-type-map workspace))))))

(defn- resolve-sub-folder [^File base-folder ^String new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(defn validate-new-folder-name [^File project-directory-file parent-path new-name]
  (let [prospect-path (str parent-path "/" new-name)]
    (when (resource-watch/reserved-proj-path? project-directory-file prospect-path)
      (format "The name %s is reserved" new-name))))

(defn new-folder? [resources]
  (and (disk-availability/available?)
       (= (count resources) 1)
       (let [resource (first resources)]
         (and (resource/editable? resource)
              (not (resource/read-only? resource))
              (some? (resource/abs-path resource))))))

(handler/defhandler :new-folder :asset-browser
  (enabled? [selection] (new-folder? selection))
  (run [selection workspace asset-browser]
    (let [parent-resource (first selection)
          parent-path (resource/proj-path parent-resource)
          parent-path (if (= parent-path "/") "" parent-path) ; special case because the project root dir ends in /
          base-folder (fs/to-folder (File. (resource/abs-path parent-resource)))
          project-directory-file (workspace/project-path workspace)
          options {:validate (partial validate-new-folder-name project-directory-file parent-path)}]
      (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder options)]
        (let [^File folder (resolve-sub-folder base-folder new-folder-name)]
          (do (fs/create-directories! folder)
              (workspace/resource-sync! workspace)
              (select-resource! asset-browser (workspace/file-resource workspace folder))))))))

(defn- selected-or-active-resource
  [selection active-resource]
  (or (handler/adapt-single selection resource/Resource)
      active-resource))

(handler/defhandler :show-in-asset-browser :global
  (active? [active-resource selection] (selected-or-active-resource selection active-resource))
  (enabled? [active-resource selection]
            (when-let [r (selected-or-active-resource selection active-resource)]
              (resource/exists? r)))
  (run [active-resource asset-browser selection main-stage]
    (when-let [r (selected-or-active-resource selection active-resource)]
      (app-view/show-asset-browser! (.getScene ^Stage main-stage))
      (select-resource! asset-browser r {:scroll? true}))))

(defn- item->path [^TreeItem item]
  (-> item (.getValue) (resource/proj-path)))

(defn- sync-tree! [old-root new-root]
  (let [item-seq (ui/tree-item-seq old-root)
        expanded (zipmap (map item->path item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (item->path item))
        (.setExpanded item true))))
  new-root)

(g/defnk produce-tree-root
  [^TreeView raw-tree-view resource-tree]
  (let [old-root (.getRoot raw-tree-view)
        new-root (tree-item resource-tree)]
    (when new-root
      (sync-tree! old-root new-root)
      (.setExpanded new-root true)
      new-root)))

(defn- auto-expand [items selected-paths]
  (not-every? false?
              (map (fn [^TreeItem item]
                     (let [path (item->path item)
                           selected (boolean (selected-paths path))
                           expanded (auto-expand (.getChildren item) selected-paths)]
                       (when expanded (.setExpanded item expanded))
                       (or selected expanded)))
                   items)))

(defn- sync-selection!
  [^TreeView tree-view selected-paths old-tree-view-paths]
  (let [root (.getRoot tree-view)
        selection-model (.getSelectionModel tree-view)]
    (.clearSelection selection-model)
    (when (and root (seq selected-paths))
      (let [selected-paths (set selected-paths)]
        (auto-expand (.getChildren root) selected-paths)
        (let [count (.getExpandedItemCount tree-view)
              selected-indices (filter #(selected-paths (item->path (.getTreeItem tree-view %))) (range count))]
          (when (not (empty? selected-indices))
            (ui/select-indices! tree-view selected-indices))
          (when-not (= old-tree-view-paths selected-paths)
            (when-some [first-item (first (.getSelectedItems selection-model))]
              (ui/scroll-to-item! tree-view first-item))))))))

(defn- update-tree-view-selection!
  [^TreeView tree-view selected-paths old-tree-view-paths]
  (sync-selection! tree-view selected-paths old-tree-view-paths)
  tree-view)

(defn- update-tree-view-root!
  [^TreeView tree-view ^TreeItem root selected-paths old-tree-view-paths]
  (when root
    (.setExpanded root true)
    (.setRoot tree-view root))
  (sync-selection! tree-view selected-paths old-tree-view-paths)
  tree-view)

(defn track-active-tab? [prefs]
  (prefs/get-prefs prefs "asset-browser-track-active-tab?" false))

(g/defnk produce-tree-view
  [^TreeView raw-tree-view ^TreeItem root active-resource prefs]
  (let [old-tree-view-paths (into #{}
                                  (map item->path)
                                  (.getSelectedItems (.getSelectionModel raw-tree-view)))
        selected-paths (or (ui/user-data raw-tree-view ::pending-selection)
                           (when (and (track-active-tab? prefs) active-resource)
                             [(resource/proj-path active-resource)])
                           (mapv resource/proj-path (ui/selection raw-tree-view)))]
    (ui/user-data! raw-tree-view ::pending-selection nil)
    (cond
      ;; different roots?
      (not (identical? (.getRoot raw-tree-view) root))
      (update-tree-view-root! raw-tree-view root selected-paths old-tree-view-paths)

      ;; same root, different selection?
      (not (= (set selected-paths) (set (map resource/proj-path (ui/selection raw-tree-view)))))
      (update-tree-view-selection! raw-tree-view selected-paths old-tree-view-paths)

      :else
      raw-tree-view)))

(defn- drag-detected [^MouseEvent e selection]
  (let [resources (roots selection)
        files (fileify-resources! resources)
        ;; Note: It would seem we should use the TransferMode/COPY_OR_MOVE mode
        ;; here in order to support making copies of non-readonly files, but
        ;; that results in every drag operation becoming a copy on macOS due to
        ;; https://bugs.openjdk.java.net/browse/JDK-8148025
        mode (if (every? (fn [resource]
                           (and (resource/editable? resource)
                                (not (resource/read-only? resource))))
                         resources)
               TransferMode/MOVE
               TransferMode/COPY)
        db (.startDragAndDrop ^Node (.getSource e) (into-array TransferMode [mode]))
        content (ClipboardContent.)]
    (when (= 1 (count resources))
      (.setDragView db (icons/get-image (workspace/resource-icon (first resources)) 16)
                    0 16))
    (.putFiles content files)
    (.setContent db content)
    (.consume e)))

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-entered [^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))]
    (when (and cell (not (.isEmpty cell)))
      (let [resource (.getValue (.getTreeItem cell))]
        (when (and (= :folder (resource/source-type resource))
                   (resource/editable? resource)
                   (not (resource/read-only? resource)))
          (ui/add-style! cell "drop-target"))))))

(defn- drag-exited [^DragEvent e]
  (when-let [cell (target (.getTarget e))]
    (ui/remove-style! cell "drop-target")))

(defn- drag-over [^DragEvent e]
  (let [db (.getDragboard e)]
    (when-let [^TreeCell cell (target (.getTarget e))]
      (when (and (not (.isEmpty cell))
                 (.hasFiles db))
       ;; Auto scrolling
       (let [view (.getTreeView cell)
             view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
             height (.getHeight (.getBoundsInLocal view))]
         (when (< view-y 15)
           (.scrollTo view (dec (.getIndex cell))))
         (when (> view-y (- height 15))
           (.scrollTo view (inc (.getIndex cell)))))
       (let [tgt-resource (-> cell (.getTreeItem) (.getValue))]
         (when (allow-resource-move? tgt-resource (.getFiles db))
           ;; Allow move only if the drag source was also the tree view.
           (if (= (.getTreeView cell) (.getGestureSource e))
             (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
             (.acceptTransferModes e (into-array TransferMode [TransferMode/COPY])))
           (.consume e)))))))

(defn- drag-move-files [dragged-pairs]
  (into [] (mapcat (fn [[src tgt]]
                     (let [src-files (doall (file-seq src))]
                       (fs/move! src tgt)
                       (moved-files src tgt src-files)))
                   dragged-pairs)))

(defn- drag-copy-files [dragged-pairs]
  (doseq [[^File src ^File tgt] dragged-pairs]
    (fs/copy! src tgt {:fail :silently}))
  [])

(defn- fixed-move-source [^File project-path [^File src ^File _tgt]]
  (contains? fixed-resource-paths (resource/file->proj-path project-path src)))

(defn drop-files! [workspace dragged-pairs move?]
  (let [project-path (workspace/project-path workspace)]
    (when (seq dragged-pairs)
      (let [moved (if move?
                    (let [{move-pairs false copy-pairs true} (group-by (partial fixed-move-source project-path) dragged-pairs)]
                      (drag-copy-files copy-pairs)
                      (drag-move-files move-pairs))
                    (drag-copy-files dragged-pairs))]
        moved))))

(defn- drag-dropped [^DragEvent e]
  (let [db (.getDragboard e)]
    (when (.hasFiles db)
      (let [target (-> e (.getTarget) ^TreeCell (target))
            tree-view (.getTreeView target)
            resource (-> target (.getTreeItem) (.getValue))
            ^Path tgt-dir-path (.toPath (fs/to-folder (File. (resource/abs-path resource))))
            move? (and (= (.getGestureSource e) tree-view)
                       (= (.getTransferMode e) TransferMode/MOVE))
            pairs (->> (.getFiles db)
                       (mapv (fn [^File f] [f (.toFile (.resolve tgt-dir-path (.getName f)))]))
                       (resolve-any-conflicts)
                       (vec))
            workspace (resource/workspace resource)
            project-path (workspace/project-path workspace)]
        (when (seq pairs)
          (let [moved (drop-files! workspace pairs move?)]
            (select-files! workspace tree-view (mapv second pairs))
            (workspace/resource-sync! workspace moved))))))
  (.setDropCompleted e true)
  (.consume e))

(defrecord SelectionProvider [asset-browser]
  handler/SelectionProvider
  (selection [this]
    (ui/selection (g/node-value asset-browser :tree-view)))
  (succeeding-selection [this]
    (let [tree-view (g/node-value asset-browser :tree-view)
          path-fn (comp #(string/split % #"/") item->path)]
      (->> (ui/selection-root-items tree-view path-fn item->path)
        (ui/succeeding-selection tree-view))))
  (alt-selection [this] []))

(defn- setup-asset-browser [asset-browser workspace ^TreeView tree-view]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [selection-provider (SelectionProvider. asset-browser)
        over-handler (ui/event-handler e (drag-over e))
        dropped-handler (ui/event-handler e (error-reporting/catch-all! (drag-dropped e)))
        detected-handler (ui/event-handler e (drag-detected e (handler/selection selection-provider)))
        entered-handler (ui/event-handler e (drag-entered e))
        exited-handler (ui/event-handler e (drag-exited e))]
    (doto tree-view
      (ui/customize-tree-view! {:double-click-expand? true})
      (ui/bind-double-click! :open)
      (ui/bind-key-commands! {"Enter" :open
                              "F2" :rename})
      (.setOnDragDetected detected-handler)
      (ui/cell-factory! (fn [resource]
                          (if (nil? resource)
                            {}
                            {:text (resource/resource-name resource)
                             :icon (workspace/resource-icon resource)
                             :style (resource/style-classes resource)
                             :over-handler over-handler
                             :dropped-handler dropped-handler
                             :entered-handler entered-handler
                             :exited-handler exited-handler})))
      (ui/register-context-menu ::resource-menu)
      (ui/context! :asset-browser {:workspace workspace :asset-browser asset-browser} selection-provider))))

(g/defnode AssetBrowser
  (property raw-tree-view TreeView)
  (property prefs g/Any)

  (input resource-tree FileResource)
  (input active-resource resource/Resource :substitute nil)

  (output root TreeItem :cached produce-tree-root)
  (output tree-view TreeView :cached produce-tree-view))

(defn make-asset-browser [graph workspace tree-view prefs]
  (let [asset-browser (first
                        (g/tx-nodes-added
                          (g/transact
                            (g/make-nodes graph
                                          [asset-browser [AssetBrowser :raw-tree-view tree-view :prefs prefs]]
                                          (g/connect workspace :resource-tree asset-browser :resource-tree)))))]
    (setup-asset-browser asset-browser workspace tree-view)
    asset-browser))
