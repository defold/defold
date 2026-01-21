;; Copyright 2020-2026 The Defold Foundation
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
  (:require [cljfx.api :as fx]
            [cljfx.fx.tree-cell :as fx.tree-cell]
            [cljfx.fx.tree-view :as fx.tree-view]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.dialogs :as dialogs]
            [editor.disk-availability :as disk-availability]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.icons :as icons]
            [editor.localization :as localization]
            [editor.menu-items :as menu-items]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.path :as path])
  (:import [com.defold.control LazyTreeItem]
           [editor.resource FileResource]
           [java.io File]
           [java.nio.file Path Paths]
           [javafx.scene.input Clipboard ClipboardContent]
           [javafx.scene.input DragEvent MouseEvent TransferMode]
           [javafx.scene Node]
           [javafx.scene.control SelectionMode TreeCell TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.stage Stage]
           [org.apache.commons.io FilenameUtils]))

;; Implementation note: tree item values are either:
;; - AssetGroup instances: grouping folders that are not implied to be resources
;; - Resource instances: resources
;; This means the selection is not guaranteed to only contain resources.

(set! *warn-on-reflection* true)

(def ^:private empty-string-array (into-array String []))

(defn- ->path [s]
  (Paths/get s empty-string-array))

(defonce/record AssetGroup [message children])

(defn- asset-group? [x]
  (instance? AssetGroup x))

(defn- list-children [asset-group-or-resource]
  (e/filter
    #(or (asset-group? %)
         (and (resource/loaded? %)
              (not (resource/internal? %))))
    (if (asset-group? asset-group-or-resource)
      (:children asset-group-or-resource)
      (resource/children asset-group-or-resource))))

(defn tree-item
  ^TreeItem [asset-group-or-resource]
  (LazyTreeItem. asset-group-or-resource list-children))

(handler/register-menu! ::resource-menu
  [menu-items/open-selected
   menu-items/open-as
   menu-items/separator
   {:label (localization/message "command.edit.copy-resource-path")
    :command :edit.copy-resource-path}
   {:label (localization/message "command.edit.copy-absolute-path")
    :command :edit.copy-absolute-path}
   {:label (localization/message "command.edit.copy-require-path")
    :command :edit.copy-require-path}
   menu-items/separator
   {:label (localization/message "command.file.show-in-desktop")
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-desktop}
   {:label (localization/message "command.file.show-references")
    :command :file.show-references}
   {:label (localization/message "command.file.show-dependencies")
    :command :file.show-dependencies}
   menu-items/separator
   menu-items/show-overrides
   menu-items/pull-up-overrides
   menu-items/push-down-overrides
   menu-items/separator
   {:label (localization/message "command.file.new")
    :command :file.new
    :expand true
    :icon "icons/64/Icons_29-AT-Unknown.png"}
   {:label (localization/message "command.file.new.any-file")
    :command :file.new
    :user-data {:any-file true}
    :icon "icons/64/Icons_29-AT-Unknown.png"}
   {:label (localization/message "command.file.new-folder")
    :command :file.new-folder
    :icon "icons/32/Icons_01-Folder-closed.png"}
   menu-items/separator
   {:label (localization/message "command.edit.cut")
    :command :edit.cut}
   {:label (localization/message "command.edit.copy")
    :command :edit.copy}
   {:label (localization/message "command.edit.paste")
    :command :edit.paste}
   {:label (localization/message "command.edit.delete")
    :command :edit.delete
    :icon "icons/32/Icons_M_06_trash.png"}
   menu-items/separator
   {:label (localization/message "command.edit.rename")
    :command :edit.rename}
   (menu-items/separator-with-id ::context-menu-end)])

(def fixed-resource-paths #{"/" "/game.project"})

(defn deletable-resource? [x]
  (and (resource/resource? x)
       (resource/editable? x)
       (not (resource/read-only? x))
       (not (fixed-resource-paths (resource/proj-path x)))))

(defn- roots [resources]
  {:pre [(coll/every? resource/resource? resources)]}
  (let [path->resource (coll/pair-map-by (comp ->path resource/proj-path) resources)
        roots (loop [paths (keys path->resource)
                     roots []]
                (if-let [^Path path (first paths)]
                  (let [roots (if (empty? (filter (fn [^Path p] (.startsWith path p)) roots))
                                (conj roots path)
                                roots)]
                    (recur (rest paths) roots))
                  roots))]
    (mapv path->resource roots)))

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
          (fs/move-to-trash! f {:fail :silently})))
      (workspace/resource-sync! workspace))))

(defn- copy [files]
  (let [cb (Clipboard/getSystemClipboard)
        content (ClipboardContent.)]
    (.putFiles content files)
    (.setContent cb content)))

(handler/defhandler :edit.copy :asset-browser
  (enabled? [selection]
    (coll/any? resource/resource? selection))
  (run [selection]
    (copy (fileify-resources! (roots (filterv resource/resource? selection))))))

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
     (when-let [tree-item (coll/first-where
                            (fn [^TreeItem tree-item]
                              (let [item (.getValue tree-item)]
                                (and (resource/resource? item)
                                     (= path (resource/proj-path item)))))
                            tree-items)]
       (doto (.getSelectionModel tree-view)
         (.clearSelection)
         (.select tree-item))
       (when scroll?
         (ui/scroll-to-item! tree-view tree-item))))))

(defn delete? [selection]
  (and (disk-availability/available?)
       (coll/any? deletable-resource? selection)))

(handler/defhandler :edit.cut :asset-browser
  (enabled? [selection] (delete? selection))
  (run [selection selection-provider asset-browser]
    (let [next (g/with-auto-evaluation-context evaluation-context
                 (-> (handler/succeeding-selection selection-provider evaluation-context)
                     (handler/adapt-single resource/Resource evaluation-context)))
          cut-files-directory (fs/create-temp-directory! "asset-cut")
          resources (roots (filterv deletable-resource? selection))]
      (copy (mapv #(temp-resource-file! cut-files-directory %) resources))
      (delete resources)
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
  [localization src-dest-pairs]
  (let [files-by-existence (group-by (fn [[src ^File dest]] (.exists dest)) src-dest-pairs)
        conflicts (get files-by-existence true)
        non-conflicts (get files-by-existence false [])]
    (if (seq conflicts)
      (when-let [strategy (dialogs/make-resolve-file-conflicts-dialog conflicts localization)]
        (into non-conflicts (resolve-conflicts strategy conflicts)))
      non-conflicts)))

(defn- select-files! [workspace tree-view files]
  (let [selected-paths (mapv (partial resource/file->proj-path (workspace/project-directory workspace)) files)]
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
             project-directory (workspace/project-directory (resource/workspace tgt-resource))]
         (and (nil? descendant)
              (nil? (some (partial reserved-project-file project-directory) possibly-reserved-tgt-files))))))

(defn paste? [files-on-clipboard? selection]
  (and files-on-clipboard?
       (disk-availability/available?)
       (= 1 (count selection))
       (let [selected-item (first selection)]
         (and (resource/resource? selected-item)
              (resource/editable? selected-item)
              (not (resource/read-only? selected-item))))))

(defn paste! [workspace target-resource src-files select-files! localization]
  (let [^File tgt-dir (reduce (fn [^File tgt ^File src]
                                (if (= tgt src)
                                  (.getParentFile ^File tgt)
                                  tgt))
                              (fs/to-folder (File. (resource/abs-path target-resource))) src-files)
        prospect-pairs (map (fn [^File f] [f (File. tgt-dir (FilenameUtils/getName (.toString f)))]) src-files)
        project-directory (workspace/project-directory workspace)]
    (if-let [illegal (illegal-copy-move-pairs project-directory prospect-pairs)]
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "dialog.asset-paste-reserved.title")
         :icon :icon/triangle-error
         :header (localization/message "dialog.asset-paste-reserved.header")
         :content (localization/message "dialog.asset-paste-reserved.content"
                                        {"directories" (string/join "\n" (map (comp (partial resource/file->proj-path project-directory) second) illegal))})})
      (let [pairs (ensure-unique-dest-files (fn [_ basename] (str basename "_copy")) prospect-pairs)]
        (doseq [[^File src-file ^File tgt-file] pairs]
          (fs/copy! src-file tgt-file {:target :merge}))
        (select-files! (mapv second pairs))
        (workspace/resource-sync! workspace)))))

(handler/defhandler :edit.paste :asset-browser
  (enabled? [selection] (paste? (.hasFiles (Clipboard/getSystemClipboard)) selection))
  (run [selection workspace asset-browser localization]
       (let [tree-view (g/node-value asset-browser :tree-view)
             resource (first selection)
             src-files (.getFiles (Clipboard/getSystemClipboard))
             dest-path (.toPath (io/file (resource/abs-path resource)))]
         (if-let [conflicting-file (some #(let [src-path (.toPath ^File %)]
                                            (when (and (.startsWith dest-path src-path)
                                                       (not= dest-path src-path))
                                              %))
                                         src-files)]
           (let [res-proj-path (resource/proj-path resource)
                 dest-proj-path (resource/file->proj-path (workspace/project-directory workspace) conflicting-file)]
             (notifications/show!
               (workspace/notifications workspace)
               {:type :error
                :id ::asset-circular-paste
                :message (localization/message
                           "notification.asset-browser.circular-paste.error"
                           {"source" dest-proj-path
                            "target" res-proj-path})}))
           (paste! workspace resource src-files (partial select-files! workspace tree-view) localization)))))

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

(defn rename? [selection]
  (and (disk-availability/available?)
       (pos? (count selection))
       (coll/every?
         (fn [selected-item]
           (and (resource/resource? selected-item)
                (resource/editable? selected-item)
                (not (resource/read-only? selected-item))
                (not (fixed-resource-paths (resource/proj-path selected-item)))))
         selection)
       (case (into #{} (map resource/source-type) selection)
         #{:folder} (= 1 (count selection))
         #{:file} (and
                    (= 1 (count (into #{} (map resource/base-name) selection)))
                    (= (count selection) (count (into #{} (map resource/ext) selection))))
         false)))

(defn rename [resources new-base-name localization]
  {:pre [(string? new-base-name) (rename? resources)]}
  (let [workspace (resource/workspace (first resources))
        project-directory (workspace/project-directory workspace)
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
                       project-directory
                       (resource/file->proj-path project-directory (val %)))
                    rename-pairs)
      ;; plain case change causes irrelevant conflict on case-insensitive file systems
      ;; fs/move! handles this, no need to resolve
      (let [{case-changes true possible-conflicts false}
            (group-by #(path/same? (key %) (val %)) rename-pairs)]
        (when-let [resolved-conflicts (resolve-any-conflicts localization possible-conflicts)]
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
      (localization/message "dialog.rename.error.reserved-name" {"name" new-name}))))

(handler/defhandler :edit.rename :asset-browser
  (enabled? [selection] (rename? selection))
  (run [selection workspace localization]
    (let [first-resource (first selection)
          dir (= :folder (resource/source-type first-resource))
          name (if dir
                 (resource/resource-name first-resource)
                 (resource/base-name first-resource))
          extensions (if dir [""] (mapv resource/ext selection))
          parent-paths (mapv (comp resource/parent-proj-path resource/proj-path) selection)
          project-directory (workspace/project-directory workspace)]
      (when-let [new-name (dialogs/make-rename-dialog
                            name
                            :localization localization
                            :title (if dir
                                     (localization/message "dialog.rename.header.folder")
                                     (localization/message "dialog.rename.header.files" {"n" (count selection)}))
                            :label (if dir
                                     (localization/message "dialog.rename.label.folder")
                                     (localization/message "dialog.rename.label.file"))
                            :extensions extensions
                            :validate (fn [file-name]
                                        (some #(validate-new-resource-name project-directory % file-name) parent-paths)))]
        (rename selection new-name localization)))))

(handler/defhandler :edit.delete :asset-browser
  (enabled? [selection] (delete? selection))
  (run [selection asset-browser selection-provider localization]
    (let [next (g/with-auto-evaluation-context evaluation-context
                 (-> (handler/succeeding-selection selection-provider evaluation-context)
                     (handler/adapt-single resource/Resource evaluation-context)))
          resources (roots (filterv deletable-resource? selection))]
      (when (if (= 1 (count resources))
              (dialogs/make-confirmation-dialog
                localization
                {:title (localization/message "dialog.asset-delete-single.title")
                 :icon :icon/circle-question
                 :header (localization/message "dialog.asset-delete-single.header"
                                               {"name" (resource/resource-name (first resources))})
                 :buttons [{:text (localization/message "dialog.button.cancel")
                            :cancel-button true
                            :default-button true
                            :result false}
                           {:text (localization/message "dialog.button.delete")
                            :variant :danger
                            :result true}]})
              (dialogs/make-info-dialog
                localization
                {:title (localization/message "dialog.asset-delete-multiple.title")
                 :icon :icon/circle-question
                 :header (localization/message "dialog.asset-delete-multiple.header")
                 :content {:text (localization/message
                                   "dialog.asset-delete-multiple.content"
                                   {"items" (->> resources
                                                 (e/map
                                                   #(localization
                                                      (localization/message "dialog.asset-delete-multiple.content.item"
                                                                            {"name" (resource/resource-name %)})))
                                                 (coll/join-to-string))})}
                 :buttons [{:text (localization/message "dialog.button.cancel")
                            :cancel-button true
                            :default-button true
                            :result false}
                           {:text (localization/message "dialog.button.delete")
                            :variant :danger
                            :result true}]}))
        (when (and (delete resources) next)
          (select-resource! asset-browser next))))))

(defn- create-template-file! [^String template ^File new-file]
  (let [base-name (FilenameUtils/getBaseName (str new-file))
        contents (workspace/replace-template-name template base-name)]
    (spit new-file contents)))

(handler/defhandler :file.new :global
  (label [user-data]
    (if-not user-data
      (localization/message "command.file.new")
      (let [rt (:resource-type user-data)]
        (or (:label rt) (:ext rt)))))
  (active? [selection selection-context evaluation-context]
    (or (= :global selection-context)
        (and (= :asset-browser selection-context)
             (= (count selection) 1)
             (some? (some-> (handler/adapt-single selection resource/Resource evaluation-context)
                      resource/abs-path)))))
  (enabled? [] (disk-availability/available?))
  (run [selection user-data asset-browser app-view prefs workspace project localization]
    (let [project-directory (workspace/project-directory workspace)
          base-folder (-> (or (some-> (g/with-auto-evaluation-context evaluation-context
                                        (handler/adapt-every selection resource/Resource evaluation-context))
                                      first
                                      resource/abs-path
                                      (File.))
                              project-directory)
                          fs/to-folder)
          rt (:resource-type user-data)
          any-file (:any-file user-data false)]
      (when-let [desired-file (dialogs/make-new-file-dialog
                                project-directory
                                base-folder
                                (when-not any-file
                                  (or (:label rt) (:ext rt)))
                                (:ext rt)
                                localization)]
        (when-let [[[_ ^File new-file]] (resolve-any-conflicts localization [[nil desired-file]])]
          (let [rt (if (and any-file (not rt))
                     (when-let [ext (second (re-find #"\.(.+)$" (.getName new-file)))]
                       (workspace/get-resource-type workspace ext))
                     rt)
                template (or (workspace/template workspace rt) "")]
            (create-template-file! template new-file))
          (workspace/resource-sync! workspace)
          (let [resource-map (g/node-value workspace :resource-map)
                new-resource-path (resource/file->proj-path project-directory new-file)
                resource (resource-map new-resource-path)]
            (when (resource/loaded? resource)
              (app-view/open-resource! app-view prefs localization project resource))
            (select-resource! asset-browser resource))))))
  (options [workspace selection user-data evaluation-context]
    (when (not user-data)
      (localization/annotate-as-sorted
        localization/natural-sort-by-label
        (into [{:label (localization/message "command.file.new.option.any-file")
                :icon "icons/64/Icons_29-AT-Unknown.png"
                :command :file.new
                :user-data {:any-file true}}]
              (keep (fn [[_ext resource-type]]
                      (when (workspace/has-template? workspace resource-type evaluation-context)
                        {:label (or (:label resource-type) (:ext resource-type))
                         :icon (:icon resource-type)
                         :style (resource/type-style-classes resource-type)
                         :command :file.new
                         :user-data {:resource-type resource-type}})))
              (resource/resource-types-by-type-ext (:basis evaluation-context) workspace :editable))))))

(defn- resolve-sub-folder [^File base-folder ^String new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(defn validate-new-folder-name [^File project-directory-file parent-path new-name]
  (let [prospect-path (str parent-path "/" new-name)]
    (when (resource-watch/reserved-proj-path? project-directory-file prospect-path)
      (format "The name %s is reserved" new-name))))

(defn new-folder? [selection]
  (and (disk-availability/available?)
       (= (count selection) 1)
       (let [selected-item (first selection)]
         (and (resource/resource? selected-item)
              (resource/editable? selected-item)
              (not (resource/read-only? selected-item))
              (some? (resource/abs-path selected-item))))))

(handler/defhandler :file.new-folder :asset-browser
  (enabled? [selection] (new-folder? selection))
  (run [selection workspace asset-browser localization]
    (let [parent-resource (first selection)
          parent-path (resource/proj-path parent-resource)
          parent-path (if (= parent-path "/") "" parent-path) ; special case because the project root dir ends in /
          base-folder (fs/to-folder (File. (resource/abs-path parent-resource)))
          project-directory (workspace/project-directory workspace)
          options {:validate (partial validate-new-folder-name project-directory parent-path)
                   :localization localization}]
      (when-let [new-folder-name (dialogs/make-new-folder-dialog options)]
        (let [^File folder (resolve-sub-folder base-folder new-folder-name)]
          (do (fs/create-directories! folder)
              (workspace/resource-sync! workspace)
              (select-resource! asset-browser (workspace/file-resource workspace folder))))))))

(defn- selected-or-active-resource
  [selection active-resource evaluation-context]
  (or (handler/adapt-single selection resource/Resource evaluation-context)
      active-resource))

(handler/defhandler :file.show-in-assets :global
  (active? [active-resource selection evaluation-context] (selected-or-active-resource selection active-resource evaluation-context))
  (enabled? [active-resource selection evaluation-context]
    (when-let [r (selected-or-active-resource selection active-resource evaluation-context)]
      (resource/exists? r)))
  (run [active-resource asset-browser selection main-stage]
    (when-let [r (g/with-auto-evaluation-context evaluation-context
                   (selected-or-active-resource selection active-resource evaluation-context))]
      (app-view/show-asset-browser! (.getScene ^Stage main-stage))
      (select-resource! asset-browser r {:scroll? true}))))

(defn- asset-group-or-resource->id [asset-group-or-resource]
  {:post [(some? %)]}
  (if (asset-group? asset-group-or-resource)
    (:message asset-group-or-resource)
    (resource/proj-path asset-group-or-resource)))

(defn- tree-item->id [^TreeItem tree-item]
  (-> tree-item .getValue asset-group-or-resource->id))

(defn- sync-tree! [old-root new-root]
  (let [expanded (->> old-root
                      ui/tree-item-seq
                      (coll/pair-map-by tree-item->id TreeItem/.isExpanded))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (tree-item->id item))
        (.setExpanded item true)))))

(def ^:private assets-message (localization/message "pane.assets"))
(def ^:private dependencies-message (localization/message "assets.dependencies"))

(defn- with-dependencies-subtree [resource-tree]
  (let [children (:children resource-tree)
        [file-resources dep-resources] (coll/separate-by resource/file-resource? children)]
    (->AssetGroup
      assets-message
      (-> []
          (cond->
            (not (coll/empty? dep-resources))
            (conj (->AssetGroup dependencies-message dep-resources)))
          (conj (assoc resource-tree :children file-resources))))))

(defn- apply-default-expansion! [^TreeItem root]
  (run!
    (fn [^TreeItem child]
      (when (resource/resource? (.getValue child))
        (.setExpanded child true)))
    (.getChildren root)))

(g/defnk produce-tree-root
  [^TreeView raw-tree-view resource-tree]
  (let [old-root (.getRoot raw-tree-view)
        new-root (tree-item (with-dependencies-subtree resource-tree))]
    (sync-tree! old-root new-root)
    (when-not old-root (apply-default-expansion! new-root))
    new-root))

(defn- auto-expand [items selected-ids]
  (not-every? false?
              (map (fn [^TreeItem item]
                     (let [id (tree-item->id item)
                           selected (boolean (selected-ids id))
                           expanded (auto-expand (.getChildren item) selected-ids)]
                       (when expanded (.setExpanded item expanded))
                       (or selected expanded)))
                   items)))

(defn- sync-selection!
  [^TreeView tree-view selected-ids old-tree-view-ids]
  (let [root (.getRoot tree-view)
        selection-model (.getSelectionModel tree-view)]
    (.clearSelection selection-model)
    (when (and root (seq selected-ids))
      (let [selected-ids (set selected-ids)]
        (auto-expand (.getChildren root) selected-ids)
        (let [count (.getExpandedItemCount tree-view)
              selected-indices (filterv #(selected-ids (tree-item->id (.getTreeItem tree-view %))) (range count))]
          (when (not (empty? selected-indices))
            (ui/select-indices! tree-view selected-indices))
          (when-not (= old-tree-view-ids selected-ids)
            (when-some [first-item (first (.getSelectedItems selection-model))]
              (ui/scroll-to-item! tree-view first-item))))))))

(defn- update-tree-view-selection!
  [^TreeView tree-view selected-ids old-tree-view-ids]
  (sync-selection! tree-view selected-ids old-tree-view-ids)
  tree-view)

(defn- update-tree-view-root!
  [^TreeView tree-view ^TreeItem root selected-ids old-tree-view-ids]
  (when root
    (.setExpanded root true)
    (.setRoot tree-view root))
  (sync-selection! tree-view selected-ids old-tree-view-ids)
  tree-view)

(defn track-active-tab? [prefs]
  (prefs/get prefs [:asset-browser :track-active-tab]))

(g/defnk produce-tree-view
  [^TreeView raw-tree-view ^TreeItem root active-resource prefs]
  (let [old-tree-view-ids (into #{}
                                (comp
                                  (remove nil?)
                                  (map tree-item->id))
                                (.getSelectedItems (.getSelectionModel raw-tree-view)))
        selected-ids (or (ui/user-data raw-tree-view ::pending-selection)
                         (when (and (track-active-tab? prefs) active-resource)
                           [(asset-group-or-resource->id active-resource)])
                         (mapv asset-group-or-resource->id (ui/selection raw-tree-view)))]
    (ui/user-data! raw-tree-view ::pending-selection nil)
    (cond
      ;; different roots?
      (not (identical? (.getRoot raw-tree-view) root))
      (update-tree-view-root! raw-tree-view root selected-ids old-tree-view-ids)

      ;; same root, different selection?
      (not (= (set selected-ids) (into #{} (map asset-group-or-resource->id) (ui/selection raw-tree-view))))
      (update-tree-view-selection! raw-tree-view selected-ids old-tree-view-ids)

      :else
      raw-tree-view)))

(defn- drag-detected [^MouseEvent e selection]
  (let [resources (roots (filterv resource/resource? selection))
        files (fileify-resources! resources)
        paths (->> resources
                   (mapv resource/proj-path)
                   (string/join "\n"))
        db (.startDragAndDrop ^Node (.getSource e) TransferMode/COPY_OR_MOVE)
        content (ClipboardContent.)]
    (when (= 1 (count resources))
      (.setDragView db (icons/get-image (workspace/resource-icon (first resources)) 16)
                    0 16))
    (.putFiles content files)
    (.putString content paths)
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
      (let [item (.getValue (.getTreeItem cell))]
        (when (and (resource/resource? item)
                   (= :folder (resource/source-type item))
                   (resource/editable? item)
                   (not (resource/read-only? item)))
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
  (let [project-directory (workspace/project-directory workspace)]
    (when (seq dragged-pairs)
      (let [moved (if move?
                    (let [{move-pairs false copy-pairs true} (group-by (partial fixed-move-source project-directory) dragged-pairs)]
                      (drag-copy-files copy-pairs)
                      (drag-move-files move-pairs))
                    (drag-copy-files dragged-pairs))]
        moved))))

(defn- drag-dropped [^DragEvent e localization]
  (let [db (.getDragboard e)]
    (when (.hasFiles db)
      (let [target (-> e (.getTarget) ^TreeCell (target))
            tree-view (.getTreeView target)
            resource (-> target (.getTreeItem) (.getValue))
            ^Path tgt-dir-path (.toPath (fs/to-folder (File. (resource/abs-path resource))))
            workspace (resource/workspace resource)
            move? (and (= (.getGestureSource e) tree-view)
                       (coll/every? (fn [^File f]
                                      (let [res (workspace/file-resource workspace f)]
                                        (and (resource/editable? res)
                                             (not (resource/read-only? res)))))
                                    (.getFiles db)))
            pairs (->> (.getFiles db)
                       (mapv (fn [^File f] [f (.toFile (.resolve tgt-dir-path (.getName f)))]))
                       (resolve-any-conflicts localization)
                       (vec))]
        (when (seq pairs)
          (let [moved (drop-files! workspace pairs move?)]
            (select-files! workspace tree-view (mapv second pairs))
            (workspace/resource-sync! workspace moved))))))
  (.setDropCompleted e true)
  (.consume e))

(defonce/record SelectionProvider [asset-browser]
  handler/SelectionProvider
  (selection [_this evaluation-context]
    (ui/selection (g/node-value asset-browser :tree-view evaluation-context)))
  (succeeding-selection [_this evaluation-context]
    (let [tree-view (g/node-value asset-browser :tree-view evaluation-context)]
      (->> (ui/selection-root-items tree-view asset-group-or-resource->id)
           (ui/succeeding-selection tree-view))))
  (alt-selection [_this _evaluation-context] []))

(defn- describe-tree-cell [localization-state on-drag-dropped item]
  (cond
    (nil? item)
    {}

    (asset-group? item)
    {:text (localization-state (:message item))
     :graphic {:fx/type ui/image-icon :path "icons/32/Icons_03-Builtins.png" :size 16.0}}

    :else
    {:text (resource/resource-name item)
     :style-class (into ["cell" "indexed-cell" "tree-cell"] (resource/style-classes item))
     :on-drag-over drag-over
     :on-drag-entered drag-entered
     :on-drag-exited drag-exited
     :on-drag-dropped on-drag-dropped
     :graphic {:fx/type ui/image-icon :path (workspace/resource-icon item) :size 16.0}}))

(def ^:private ext-with-tree-view-props
  (fx/make-ext-with-props fx.tree-view/props))

(fxui/defc asset-tree-view
  {:compose [{:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}]}
  [{:keys [tree-view localization-state on-drag-dropped]}]
  {:fx/type ext-with-tree-view-props
   :desc {:fx/type fxui/ext-value :value tree-view}
   :props {:cell-factory {:fx/cell-type fx.tree-cell/lifecycle
                          :describe (fn/partial #'describe-tree-cell localization-state on-drag-dropped)}}})

(defn- setup-asset-browser [asset-browser workspace ^TreeView tree-view localization]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [selection-provider (SelectionProvider. asset-browser)
        detected-handler (ui/event-handler e (drag-detected e (g/with-auto-evaluation-context evaluation-context (handler/selection selection-provider evaluation-context))))
        original-dispatcher (.getEventDispatcher tree-view)]
    (fx/create-component
      {:fx/type asset-tree-view
       :tree-view tree-view
       :localization localization
       :on-drag-dropped #(error-reporting/catch-all! (drag-dropped % localization))})
    (doto tree-view
      (.setShowRoot false)
      (ui/customize-tree-view! {:double-click-expand? true})
      (ui/bind-double-click! :file.open-selected)
      (ui/bind-key-commands! {"Enter" :file.open-selected})
      (.setEventDispatcher
        (ui/event-dispatcher event tail
           ;; by default, TreeView handles F2 as an edit operation. We override
           ;; the dispatcher here to bubble up the F2 key presses so they are
           ;; still handled upstream (e.g. if we use F2 shortcut for rename,
           ;; which is a default)
           (if (instance? KeyEvent event)
             (let [^KeyEvent event event]
               (if (and (= KeyCode/F2 (.getCode event))
                        (or (= KeyEvent/KEY_PRESSED (.getEventType event))
                            (= KeyEvent/KEY_RELEASED (.getEventType event))))
                 event
                 (.dispatchEvent original-dispatcher event tail)))
             (.dispatchEvent original-dispatcher event tail))))
      (.setOnDragDetected detected-handler)
      (ui/register-context-menu ::resource-menu)
      (ui/context! :asset-browser {:workspace workspace :asset-browser asset-browser :localization localization} selection-provider))))

(g/defnode AssetBrowser
  (property raw-tree-view TreeView)
  (property prefs g/Any)

  (input resource-tree FileResource)
  (input active-resource resource/Resource :substitute nil)

  (output root TreeItem :cached produce-tree-root)
  (output tree-view TreeView :cached produce-tree-view))

(defn make-asset-browser [graph workspace tree-view prefs localization]
  (let [asset-browser (first
                        (g/tx-nodes-added
                          (g/transact
                            (g/make-nodes graph
                                          [asset-browser [AssetBrowser :raw-tree-view tree-view :prefs prefs]]
                                          (g/connect workspace :resource-tree asset-browser :resource-tree)))))]
    (setup-asset-browser asset-browser workspace tree-view localization)
    asset-browser))
