;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.outline-view
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.error-reporting :as error-reporting]
            [editor.handler :as handler]
            [editor.icons :as icons]
            [editor.keymap :as keymap]
            [editor.menu-items :as menu-items]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.scene-visibility :as scene-visibility]
            [editor.types :as types]
            [editor.ui :as ui])
  (:import [com.defold.control TreeCell]
           [java.awt Toolkit]
           [javafx.collections FXCollections ObservableList]
           [javafx.event Event]
           [javafx.geometry Orientation]
           [javafx.scene Node]
           [javafx.scene.control ScrollBar SelectionMode TreeItem TreeView ToggleButton Label TextField]
           [javafx.scene.image ImageView]
           [javafx.scene.input Clipboard ContextMenuEvent DataFormat DragEvent KeyCode KeyCodeCombination KeyEvent MouseButton MouseEvent TransferMode]
           [javafx.scene.layout AnchorPane HBox Priority]
           [javafx.util Callback]))

(set! *warn-on-reflection* true)

(declare tree-item)

(def ^:private ^:dynamic *programmatic-selection* nil)
(def ^:private ^:dynamic *paste-into-parent* nil)

;; TreeItem creator
(defn- list-children ^ObservableList [parent]
  (let [children (:children parent)
        items (into-array TreeItem (map tree-item children))]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll ^"[Ljavafx.scene.control.TreeItem;" items)))))

;; NOTE: Without caching stack-overflow... WHY?
(defn tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children parent)))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children parent)))
          children))
      (outline/value [^TreeItem this]
        (.getValue this))
      (outline/parent [^TreeItem this]
        (.getParent this)))))

(defrecord TreeItemIterator [^TreeItem tree-item]
  outline/ItemIterator
  (value [this] (.getValue tree-item))
  (parent [this] (when (.getParent tree-item)
                   (TreeItemIterator. (.getParent tree-item)))))

(defn- ->iterator [^TreeItem tree-item]
  (TreeItemIterator. tree-item))

(defn- map-filter [filter-fn m]
  (into {} (filter filter-fn m)))

(defn- item->node-id [^TreeItem item]
  (:node-id (.getValue item)))

(defn- sync-tree [old-root new-root]
  (let [item-seq (ui/tree-item-seq old-root)
        expanded (zipmap (map item->node-id item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (item->node-id item))
        (.setExpanded item true))))
  new-root)

(defn- auto-expand [items selected-ids]
  (reduce #(or %1 %2) false (map (fn [^TreeItem item] (let [id (item->node-id item)
                                                            selected (boolean (selected-ids id))
                                                            expanded (auto-expand (.getChildren item) selected-ids)]
                                                        (when expanded (.setExpanded item expanded))
                                                        (or selected expanded))) items)))

(defn- sync-selection [^TreeView tree-view selection old-selected-ids]
  (let [root (.getRoot tree-view)
        selection-model (.getSelectionModel tree-view)]
    (.clearSelection selection-model)
    (when (and root (not (empty? selection)))
      (let [selected-ids (set selection)]
        (when (auto-expand (.getChildren root) selected-ids)
          (.setExpanded root true))
        (let [count (.getExpandedItemCount tree-view)
              selected-indices (filter #(selected-ids (item->node-id (.getTreeItem tree-view %))) (range count))]
          (when (not (empty? selected-indices))
            (ui/select-indices! tree-view selected-indices))
          (when-not (= old-selected-ids selected-ids)
            (when-some [first-item (first (.getSelectedItems selection-model))]
              (ui/scroll-to-item! tree-view first-item))))))))

(defn- decorate
  ([hidden-node-outline-key-paths root outline-name-paths]
   (:item (decorate hidden-node-outline-key-paths [] [] root (:outline-reference? root) outline-name-paths)))
  ([hidden-node-outline-key-paths node-id-path node-outline-key-path {:keys [node-id] :as item} parent-reference? outline-name-paths]
   (let [node-id-path (conj node-id-path node-id)
         node-outline-key-path (if (empty? node-outline-key-path)
                                 [node-id]
                                 (if-some [node-outline-key (:node-outline-key item)]
                                   (conj node-outline-key-path node-outline-key)
                                   node-outline-key-path))
         data (mapv #(decorate hidden-node-outline-key-paths node-id-path node-outline-key-path % (or parent-reference? (:outline-reference? item)) outline-name-paths) (:children item))
         item (assoc item
                :node-id-path node-id-path
                :node-outline-key-path node-outline-key-path
                :parent-reference? parent-reference?
                :children (mapv :item data)
                :child-error? (boolean (some :child-error? data))
                :child-overridden? (boolean (some :child-overridden? data))
                :hideable? (contains? outline-name-paths (rest node-outline-key-path))
                :hidden-parent? (scene-visibility/hidden-outline-key-path? hidden-node-outline-key-paths node-outline-key-path)
                :scene-visibility (if (contains? hidden-node-outline-key-paths node-outline-key-path)
                                    :hidden
                                    :visible))]
     {:item item
      :child-error? (or (:child-error? item) (:outline-error? item))
      :child-overridden? (or (:child-overridden? item) (:outline-overridden? item))})))

(g/defnk produce-tree-root
  [active-outline active-resource-node open-resource-nodes raw-tree-view hidden-node-outline-key-paths outline-name-paths]
  (let [resource-node-set (set open-resource-nodes)
        root-cache (or (ui/user-data raw-tree-view ::root-cache) {})
        [root outline old-hidden-node-outline-key-paths] (get root-cache active-resource-node)
        new-root (if (or (not= outline active-outline)
                         (not= old-hidden-node-outline-key-paths hidden-node-outline-key-paths)
                         (and (nil? root) (nil? outline)))
                   (sync-tree root (tree-item (decorate hidden-node-outline-key-paths active-outline outline-name-paths)))
                   root)
        new-cache (assoc (map-filter (fn [[resource-node _]]
                                       (contains? resource-node-set resource-node))
                                     root-cache)
                    active-resource-node [new-root active-outline hidden-node-outline-key-paths])]
    (ui/user-data! raw-tree-view ::root-cache new-cache)
    new-root))

(defn- update-tree-view-selection!
  [^TreeView tree-view selection old-selected-ids]
  (binding [*programmatic-selection* true]
    (sync-selection tree-view selection old-selected-ids)
    tree-view))

(defn- update-tree-view-root!
  [^TreeView tree-view ^TreeItem root selection old-selected-ids]
  (binding [*programmatic-selection* true]
    (when (not (identical? (.getRoot tree-view) root))
      (when root
        (.setExpanded root true)
        (.setRoot tree-view root)))
    (sync-selection tree-view selection old-selected-ids)
    tree-view))

(g/defnk update-tree-view [^TreeView raw-tree-view ^TreeItem root selection]
  (let [tree-view-ids (into #{}
                            (map item->node-id)
                            (.getSelectedItems (.getSelectionModel raw-tree-view)))]
    (if (identical? (.getRoot raw-tree-view) root)
      (if (= (set selection) (set (map :node-id (ui/selection raw-tree-view))))
        raw-tree-view
        (update-tree-view-selection! raw-tree-view selection tree-view-ids))
      (update-tree-view-root! raw-tree-view root selection tree-view-ids))))

(defn- item->value [^TreeItem item]
  (.getValue item))

(defn- alt-selection [selection]
  (let [alt-selection (mapv (fn [item]
                              (:alt-outline item item))
                            selection)]
    (if (not= selection alt-selection)
      alt-selection
      [])))

(g/defnode OutlineView
  (property raw-tree-view TreeView)

  (input active-outline g/Any :substitute {})
  (input active-resource-node g/NodeID :substitute nil)
  (input open-resource-nodes g/Any :substitute [])
  (input selection g/Any :substitute [])
  (input hidden-node-outline-key-paths types/NodeOutlineKeyPaths)
  (input outline-name-paths scene-visibility/OutlineNamePaths)

  (output root TreeItem :cached produce-tree-root)
  (output tree-view TreeView :cached update-tree-view)
  (output tree-selection g/Any :cached (g/fnk [tree-view] (ui/selection tree-view)))
  (output tree-selection-root-its g/Any :cached (g/fnk [tree-view] (mapv ->iterator (ui/selection-root-items tree-view (comp :node-id-path item->value) (comp :node-id item->value)))))
  (output succeeding-tree-selection g/Any :cached (g/fnk [tree-view tree-selection-root-its]
                                                         (->> tree-selection-root-its
                                                           (mapv :tree-item)
                                                           (ui/succeeding-selection tree-view))))
  (output alt-tree-selection g/Any :cached (g/fnk [tree-selection]
                                                  (alt-selection tree-selection))))

(handler/register-menu! ::outline-menu
  [menu-items/open-selected
   menu-items/open-as
   menu-items/separator
   {:label "Copy Resource Path"
    :command :edit.copy-resource-path}
   {:label "Copy Full Path"
    :command :edit.copy-absolute-path}
   {:label "Copy Require Path"
    :command :edit.copy-require-path}
   menu-items/separator
   {:label "Show in Asset Browser"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-assets}
   {:label "Show in Desktop"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-desktop}
   {:label "Referencing Files..."
    :command :file.show-references}
   {:label "Dependencies..."
    :command :file.show-dependencies}
   menu-items/separator
   menu-items/show-overrides
   menu-items/pull-up-overrides
   menu-items/push-down-overrides
   menu-items/separator
   {:label "Add"
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-embedded-component
    :expand true}
   {:label "Add From File"
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-referenced-component}
   {:label "Add Secondary"
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-secondary-embedded-component}
   {:label "Add Secondary From File"
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-secondary-referenced-component}
   menu-items/separator
   {:label "Cut"
    :command :edit.cut}
   {:label "Copy"
    :command :edit.copy}
   {:label "Paste"
    :command :edit.paste}
   {:label "Delete"
    :icon "icons/32/Icons_M_06_trash.png"
    :command :edit.delete}
   menu-items/separator
   {:label "Rename..."
    :command :edit.rename}
   menu-items/separator
   {:label "Move Up"
    :command :edit.reorder-up}
   {:label "Move Down"
    :command :edit.reorder-down}
   menu-items/separator
   {:label "Show/Hide Objects"
    :command :scene.visibility.toggle-selection}
   {:label "Hide Unselected Objects"
    :command :scene.visibility.hide-unselected}
   {:label "Show Last Hidden Objects"
    :command :scene.visibility.show-last-hidden}
   {:label "Show All Hidden Objects"
    :command :scene.visibility.show-all}
   (menu-items/separator-with-id ::context-menu-end)])

(defn- selection->nodes [selection]
  (handler/adapt-every selection Long))

(defn- root-iterators
  ([outline-view]
   (g/with-auto-evaluation-context evaluation-context
     (root-iterators outline-view evaluation-context)))
  ([outline-view evaluation-context]
   (g/node-value outline-view :tree-selection-root-its evaluation-context)))

(handler/defhandler :edit.delete :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection outline-view evaluation-context]
            (and (< 0 (count selection))
                 (-> (root-iterators outline-view evaluation-context)
                   outline/delete?)))
  (run [app-view selection selection-provider outline-view]
       (let [next (-> (handler/succeeding-selection selection-provider)
                    handler/selection->node-ids)]
         (g/transact
           (concat
             (g/operation-label "Delete")
             (for [node-id (handler/selection->node-ids selection)]
               (do
                 (g/delete-node (g/override-root node-id))))
             (when (seq next)
               (app-view/select app-view next)))))))

(def data-format-fn (fn []
                      (let [json "application/json"]
                        (or (DataFormat/lookupMimeType json)
                            (DataFormat. (into-array String [json]))))))

;; Iff every item-iterator has the same parent, return that parent, else nil
(defn- single-parent-it [item-iterators]
  (when (not-empty item-iterators)
    (let [parents (map outline/parent item-iterators)]
      (when (apply = parents)
        (first parents)))))

(defn- set-paste-parent! [root-its]
  (alter-var-root #'*paste-into-parent* (constantly (some-> (single-parent-it root-its) outline/value :node-id))))

(handler/defhandler :edit.copy :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection] (< 0 (count selection)))
  (run [outline-view project]
       (let [root-its (root-iterators outline-view)
             cb (Clipboard/getSystemClipboard)
             data (outline/copy project root-its)]
         (set-paste-parent! root-its)
         (.setContent cb {(data-format-fn) data}))))

(defn- paste-target-it [item-iterators]
  (let [single-parent (single-parent-it item-iterators)]
    (or (when (= (some-> single-parent outline/value :node-id) *paste-into-parent*)
          single-parent)
      (first item-iterators))))

(handler/defhandler :edit.paste :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [project selection outline-view evaluation-context]
            (let [cb (Clipboard/getSystemClipboard)
                  target-item-it (paste-target-it (root-iterators outline-view evaluation-context))
                  data-format (data-format-fn)]
              (and target-item-it
                   (.hasContent cb data-format)
                   (outline/paste? project target-item-it (.getContent cb data-format)))))
  (run [project outline-view app-view]
    (let [target-item-it (paste-target-it (root-iterators outline-view))
          cb (Clipboard/getSystemClipboard)
          data-format (data-format-fn)]
      (outline/paste! project target-item-it (.getContent cb data-format) (partial app-view/select app-view))
      (set-paste-parent! (root-iterators outline-view)))))

(handler/defhandler :edit.cut :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection outline-view evaluation-context]
            (let [item-iterators (root-iterators outline-view evaluation-context)]
              (and (< 0 (count item-iterators))
                   (outline/cut? item-iterators))))
  (run [app-view selection-provider outline-view project]
       (let [item-iterators (root-iterators outline-view)
             cb (Clipboard/getSystemClipboard)
             data-format (data-format-fn)
             next (-> (handler/succeeding-selection selection-provider)
                    handler/selection->node-ids)]
         (.setContent cb {data-format (outline/cut! project item-iterators (if next (app-view/select app-view next)))}))))

(defn- dump-mouse-event [^MouseEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e)))

(defn- dump-drag-event [^DragEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e))
  (prn "ges-src" (.getGestureSource e))
  (prn "ges-tgt" (.getGestureTarget e)))

(defn- drag-detected [project outline-view ^MouseEvent e]
  (let [item-iterators (root-iterators outline-view)]
    (when (outline/drag? item-iterators)
      (let [db (.startDragAndDrop ^Node (.getSource e) (into-array TransferMode TransferMode/COPY_OR_MOVE))
            data (outline/copy project item-iterators)]
        (when-let [icon (and (= 1 (count item-iterators))
                             (:icon (outline/value (first item-iterators))))]
          (.setDragView db (icons/get-image icon 16) 0 16))
        (.setContent db {(data-format-fn) data})
        (.consume e)))))

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-over [project outline-view ^DragEvent e]
  (if (not (instance? TreeCell (.getTarget e)))
    (when-let [parent (.getParent ^Node (.getTarget e))]
      (Event/fireEvent parent (.copyFor e (.getSource e) parent)))
    (let [^TreeCell cell (.getTarget e)]
      ;; Auto scrolling
      (let [view (.getTreeView cell)
            view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
            height (.getHeight (.getBoundsInLocal view))]
        (when (< view-y 15)
          (.scrollTo view (dec (.getIndex cell))))
        (when (> view-y (- height 15))
          (.scrollTo view (inc (.getIndex cell)))))
      (let [db (.getDragboard e)]
        (when (and (instance? TreeCell cell)
                   (not (.isEmpty cell))
                   (.hasContent db (data-format-fn)))
          (let [item-iterators (if (ui/drag-internal? e)
                                 (root-iterators outline-view)
                                 [])]
            (when (outline/drop? project item-iterators (->iterator (.getTreeItem cell))
                                 (.getContent db (data-format-fn)))
              (let [modes (if (ui/drag-internal? e)
                            [TransferMode/MOVE]
                            [TransferMode/COPY])]
                (.acceptTransferModes e (into-array TransferMode modes)))
              (.consume e))))))))

(defn- drag-dropped [project app-view outline-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (let [item-iterators (if (ui/drag-internal? e)
                           (root-iterators outline-view)
                           [])]
      (when (outline/drop! project item-iterators (->iterator (.getTreeItem cell))
                           (.getContent db (data-format-fn)) (partial app-view/select app-view))
        (.setDropCompleted e true)
        (.consume e)))))

(defn- drag-entered [project outline-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (when (and cell
               (not (.isEmpty cell))
               (.hasContent db (data-format-fn)))
      (let [item-iterators (if (ui/drag-internal? e)
                             (root-iterators outline-view)
                             [])]
        (when (outline/drop? project item-iterators (->iterator (.getTreeItem cell))
                             (.getContent db (data-format-fn)))
          (ui/add-style! cell "drop-target")))

      (let [future (ui/->future 0.5 (fn []
                                      (-> cell (.getTreeItem) (.setExpanded true))
                                      (ui/user-data! cell :future-expand nil)))]
        (ui/user-data! cell :future-expand future)))))

(defn- drag-exited [^DragEvent e]
  (when-let [cell (target (.getTarget e))]
    (ui/remove-style! cell "drop-target")
    (when-let [future (ui/user-data cell :future-expand)]
      (ui/cancel future)
      (ui/user-data! cell :future-expand nil))))

(defn- toggle-visibility! [node-outline-key-path ^MouseEvent event]
  (ui/run-command (.getSource event) :private/hide-toggle {:node-outline-key-path node-outline-key-path}))

(defn add-scroll-listeners!
  [visibility-button ^TreeView tree-view]
  (when-let [^ScrollBar scrollbar (->> (.lookupAll tree-view ".scroll-bar")
                                       (some #(when (= (.getOrientation ^ScrollBar %) Orientation/HORIZONTAL) %)))]
    (ui/observe (.valueProperty scrollbar) (fn [_ _ new-v] (AnchorPane/setRightAnchor visibility-button (- (.getMax scrollbar) new-v))))
    (ui/observe (.maxProperty scrollbar) (fn [_ _ new-v] (AnchorPane/setRightAnchor visibility-button (- new-v (.getValue scrollbar)))))
    (ui/observe (.visibleProperty scrollbar) (fn [_ _ visible?] (if visible?
                                                                  (AnchorPane/setRightAnchor visibility-button (.getMax scrollbar))
                                                                  (AnchorPane/setRightAnchor visibility-button 0.0))))))

(defn- set-editing-id!
  [^TreeView tree-view node-id]
  (let [editing-id (ui/user-data tree-view ::editing-id)]
    (when (not= editing-id node-id)
      (doto tree-view
        (ui/user-data! ::editing-id node-id)
        (.refresh)
        (.requestFocus)))))

(defn- activate-editing!
  [^Label label ^TextField text-field]
  (when-let [node-id (ui/user-data text-field ::node-id)]
    (when-let [id (g/maybe-node-value node-id :id)]
      (.setVisible label false)
      (doto text-field
        (.setText id)
        (.setVisible true)
        (.requestFocus)
        (.selectAll)))))

(defn- deactivate-editing!
  [^Label label ^TextField text-field]
  (.setVisible text-field false)
  (.setVisible label true))

(defn- commit-edit!
  [^TreeView tree-view ^TextField text-field]
  (let [new-text (.getText text-field)
        node-id (ui/user-data text-field ::node-id)]
    (when-not (empty? new-text)
      (g/set-property! node-id :id new-text))
    (set-editing-id! tree-view nil)))

(defn- get-selected-node-id
  [^TreeView tree-view]
  (let [selection-model (.getSelectionModel tree-view)
        selected-items (.getSelectedItems selection-model)]
    (when-let [^TreeItem selected-item (first selected-items)]
      (item->node-id selected-item))))

(defn- cancel-rename!
  [^TreeView tree-view]
  (when-let [future (ui/user-data tree-view ::future-rename)]
    (ui/cancel future)
    (ui/user-data! tree-view ::future-rename nil)))

(defn- click-handler!
  [^TreeView tree-view ^Label text-label ^MouseEvent event]
  (let [current-click-time (System/currentTimeMillis)
        last-click-time (or (ui/user-data text-label ::last-click-time) 0)
        time-diff (- current-click-time last-click-time)
        db-click-threshold (or (-> (Toolkit/getDefaultToolkit)
                                   (.getDesktopProperty "awt.multiClickInterval"))
                               500)]
    (when (= MouseButton/PRIMARY (.getButton event))
      (ui/user-data! text-label ::last-click-time current-click-time)
      (cond
        (< time-diff db-click-threshold)
        (do
          (cancel-rename! tree-view)
          (ui/run-command (.getSource event) :file.open-selected {} false (fn [] (.consume event))))

        (= (get-selected-node-id tree-view)
           (ui/user-data text-label ::node-id))
        (let [future-rename (ui/->future (/ db-click-threshold 1000)
                                         (fn []
                                           (ui/user-data! tree-view ::future-rename nil)
                                           (ui/run-command (.getSource event) :edit.rename)))]
          (ui/user-data! tree-view ::future-rename future-rename))

        :else nil))))

(defn- editable-id?
  [node-id]
  (let [node (g/node-by-id node-id)]
    (and (g/maybe-node-value node-id :id)
         (g/with-auto-evaluation-context evaluation-context
           (and (not (g/node-property-dynamic node :id :read-only? false evaluation-context))
                (g/node-property-dynamic node :id :visible true evaluation-context))))))

(handler/defhandler :edit.rename :outline
  (active? [selection outline-view]
           (and (= 1 (count selection))
                (when-let [node-id (-> (g/node-value outline-view :raw-tree-view)
                                       (get-selected-node-id))]
                  (editable-id? node-id))))
  (run [outline-view]
       (let [^TreeView tree-view (g/node-value outline-view :raw-tree-view)]
         (set-editing-id! tree-view (get-selected-node-id tree-view)))))

(def eye-open-path (ui/load-svg-path "scene/images/eye_open.svg"))
(def eye-closed-path (ui/load-svg-path "scene/images/eye_closed.svg"))

(defn make-tree-cell
  [^TreeView tree-view drag-entered-handler drag-exited-handler]
  (let [eye-icon-open (icons/make-svg-icon-graphic eye-open-path)
        eye-icon-closed (icons/make-svg-icon-graphic eye-closed-path)
        image-view-icon (ImageView.)
        visibility-button (doto (ToggleButton.)
                            (ui/add-style! "visibility-toggle")
                            (AnchorPane/setRightAnchor 0.0))
        text-label (Label.)
        text-field (TextField.)
        update-fn (fn [_] (commit-edit! tree-view text-field))
        text-field (doto text-field
                     (.setVisible false)
                     (AnchorPane/setLeftAnchor 0.0)
                     (AnchorPane/setRightAnchor 0.0)
                     (ui/on-action! update-fn)
                     (ui/on-cancel! identity)
                     (ui/auto-commit! update-fn)
                     (ui/on-focus! (fn [got-focus] (when-not got-focus
                                                     (set-editing-id! tree-view nil)))))
        _ (.addEventFilter text-label MouseEvent/MOUSE_PRESSED (ui/event-handler e (click-handler! tree-view text-label e)))
        label-pane (doto (AnchorPane. (ui/node-array [text-label text-field]))
                     (ui/add-style! "anchor-pane")
                     (HBox/setHgrow Priority/ALWAYS))
        h-box (doto (HBox. 5 (ui/node-array [image-view-icon label-pane]))
                (ui/add-style! "h-box")
                (AnchorPane/setRightAnchor 0.0)
                (AnchorPane/setLeftAnchor 0.0))
        pane (doto (AnchorPane. (ui/node-array [h-box visibility-button]))
               (ui/add-style! "anchor-pane"))
        _ (add-scroll-listeners! visibility-button tree-view)
        cell (proxy [TreeCell] []
               (updateItem [item empty]
                 (let [this ^TreeCell this]
                   (proxy-super updateItem item empty)
                   (ui/update-tree-cell-style! this)
                   (if empty
                     (do
                       (proxy-super setText nil)
                       (proxy-super setGraphic nil)
                       (proxy-super setContextMenu nil)
                       (proxy-super setStyle nil))
                     (let [{:keys [node-id label icon link color outline-error? outline-overridden? outline-reference? outline-show-link? parent-reference? child-error? child-overridden? scene-visibility hideable? node-outline-key-path hidden-parent?]} item
                           icon (if outline-error? "icons/32/Icons_E_02_error.png" icon)
                           show-link? (and (some? link)
                                           (or outline-reference? outline-show-link?))
                           text (if show-link? (format "%s - %s" label (resource/resource->proj-path link)) label)
                           hidden? (= :hidden scene-visibility)
                           visibility-icon (if hidden? eye-icon-closed eye-icon-open)
                           editing? (= node-id (ui/user-data tree-view ::editing-id))]
                       (.setImage image-view-icon (icons/get-image icon))
                       (.setText text-label text)
                       (ui/user-data! text-field ::node-id node-id)
                       (ui/user-data! text-label ::node-id node-id)
                       (doto visibility-button
                         (.setGraphic visibility-icon)
                         (ui/on-click! (partial toggle-visibility! node-outline-key-path)))
                       (proxy-super setGraphic pane)
                       (when-let [[r g b a] color]
                         (proxy-super setStyle (format "-fx-text-fill: rgba(%d, %d, %d %d);" (int (* 255 r)) (int (* 255 g)) (int (* 255 b)) (int (* 255 a)))))
                       (if parent-reference?
                         (ui/add-style! this "parent-reference")
                         (ui/remove-style! this "parent-reference"))
                       (if outline-reference?
                         (ui/add-style! this "reference")
                         (ui/remove-style! this "reference"))
                       (if outline-error?
                         (ui/add-style! this "error")
                         (ui/remove-style! this "error"))
                       (if outline-overridden?
                         (ui/add-style! this "overridden")
                         (ui/remove-style! this "overridden"))
                       (if child-error?
                         (ui/add-style! this "child-error")
                         (ui/remove-style! this "child-error"))
                       (if child-overridden?
                         (ui/add-style! this "child-overridden")
                         (ui/remove-style! this "child-overridden"))
                       (if hideable?
                         (ui/add-style! this "hideable")
                         (ui/remove-style! this "hideable"))
                       (if hidden-parent?
                         (ui/add-style! this "hidden-parent")
                         (ui/remove-style! this "hidden-parent"))
                       (if hidden?
                         (ui/add-style! this "scene-visibility-hidden")
                         (ui/remove-style! this "scene-visibility-hidden"))
                       (if editing?
                         (activate-editing! text-label text-field)
                         (deactivate-editing! text-label text-field)))))))]
    (doto cell
      (.setOnDragEntered drag-entered-handler)
      (.setOnDragExited drag-exited-handler))))

(defrecord SelectionProvider [outline-view]
  handler/SelectionProvider
  (selection [this]
    (g/node-value outline-view :tree-selection))
  (succeeding-selection [this]
    (g/node-value outline-view :succeeding-tree-selection))
  (alt-selection [this]
    (g/node-value outline-view :alt-tree-selection)))

(defn- propagate-selection [selected-items app-view]
  (when-not *programmatic-selection*
    (set-paste-parent! nil)
    (when-let [selection (into [] (keep :node-id) selected-items)]
      ;; TODO - handle selection order
      (app-view/select! app-view selection))))

(defn key-pressed-handler!
  [app-view ^TreeView tree-view ^KeyEvent event]
  (let [keymap (g/node-value app-view :keymap)
        rename-shortcuts (keymap/shortcuts keymap :edit.rename)
        editing-id (ui/user-data tree-view ::editing-id)]
    (condp = (.getCode event)
      KeyCode/ENTER
      ;; We want to commit the rename on Enter, while editing.
      (when-not editing-id
        (.consume event)
        (ui/run-command (.getSource event) :file.open-selected))
      
      ;; The key-down `F2` event is consumed by javafx, even though the built-in editing
      ;; that uses it is set to false, so `:edit.rename :outline` handler will not work
      ;; for `F2`. Since the shortcut is customizable, we need to check if it exists.
      KeyCode/F2
      (when (some #(= "F2" (.toString ^KeyCodeCombination %)) rename-shortcuts)
        (.consume event)
        (ui/run-command (.getSource event) :edit.rename))

      nil)))

(defn- setup-tree-view [project ^TreeView tree-view outline-view app-view]
  (let [drag-entered-handler (ui/event-handler e (drag-entered project outline-view e))
        drag-exited-handler (ui/event-handler e (drag-exited e))]
    (doto tree-view
      (ui/customize-tree-view! {:double-click-expand? true})
      (.. getSelectionModel (setSelectionMode SelectionMode/MULTIPLE))
      (.setOnDragDetected (ui/event-handler e (drag-detected project outline-view e)))
      (.setOnDragOver (ui/event-handler e (drag-over project outline-view e)))
      (.setOnDragDropped (ui/event-handler e (error-reporting/catch-all! (drag-dropped project app-view outline-view e))))
      (.setCellFactory (reify Callback (call ^TreeCell [this view] (make-tree-cell view drag-entered-handler drag-exited-handler))))
      (ui/observe-selection #(propagate-selection %2 app-view))
      (ui/register-context-menu ::outline-menu)
      (.addEventHandler ContextMenuEvent/CONTEXT_MENU_REQUESTED (ui/event-handler event (cancel-rename! tree-view)))
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (key-pressed-handler! app-view tree-view event)))
      (ui/context! :outline {:outline-view outline-view} (SelectionProvider. outline-view) {} {java.lang.Long :node-id
                                                                                               resource/Resource :link}))))

(defn make-outline-view [view-graph project tree-view app-view]
  (let [outline-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph OutlineView :raw-tree-view tree-view))))]
    (setup-tree-view project tree-view outline-view app-view)
    outline-view))
