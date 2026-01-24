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

(ns editor.outline-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.tree-view :as fx.ext.tree-view]
            [cljfx.fx.tree-item :as fx.tree-item]
            [cljfx.fx.tree-view :as fx.tree-view]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.icons :as icons]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.menu-items :as menu-items]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.scene-visibility :as scene-visibility]
            [editor.types :as types]
            [editor.ui :as ui]
            [internal.util :as util]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.eduction :as e])
  (:import [com.defold.control ExtendedTreeViewSkin TreeCell]
           [java.awt Toolkit]
           [javafx.geometry Orientation]
           [javafx.scene Node]
           [javafx.scene.control Label ScrollBar SelectionMode TextField ToggleButton TreeItem TreeView]
           [javafx.scene.image ImageView]
           [javafx.scene.input Clipboard ContextMenuEvent DataFormat DragEvent KeyCode KeyCodeCombination KeyEvent MouseButton MouseEvent TransferMode]
           [javafx.scene.layout AnchorPane HBox Priority]))

(set! *warn-on-reflection* true)

(def ^:private ^:dynamic *paste-into-parent* nil)

(extend-protocol outline/ItemIterator
  TreeItem
  (value [this] (.getValue this))
  (parent [this] (.getParent this)))

(defn- item->node-id [^TreeItem item]
  (:node-id (.getValue item)))

(defn- auto-expand [selected-node-id->node-id-paths]
  (->> selected-node-id->node-id-paths
       (e/mapcat val)
       (e/mapcat #(e/take-while coll/not-empty (iterate pop (pop %))))
       (reduce
         (fn [acc node-id-path]
           (assoc! acc node-id-path true))
         (transient {}))
       persistent!))

(defn decorate-outline [outline hidden-node-outline-key-paths outline-name-paths localization-state selection]
  (letfn [(decorate [{:keys [node-id children] :as outline} node-id-path node-outline-key-path parent-reference?]
            (let [node-id-path (conj node-id-path node-id)
                  node-outline-key-path (if (empty? node-outline-key-path)
                                          [node-id]
                                          (if-some [node-outline-key (:node-outline-key outline)]
                                            (conj node-outline-key-path node-outline-key)
                                            node-outline-key-path))
                  data (->> children
                            (localization/sort-if-annotated localization-state)
                            (mapv #(decorate %
                                             node-id-path
                                             node-outline-key-path
                                             (or parent-reference? (:outline-reference? outline)))))
                  outline (assoc outline
                            :node-id-path node-id-path
                            :node-outline-key-path node-outline-key-path
                            :parent-reference? parent-reference?
                            :children (mapv :outline data)
                            :child-error (boolean (coll/some :child-error data))
                            :child-overridden (boolean (coll/some :child-overridden data))
                            :hideable (contains? outline-name-paths (rest node-outline-key-path))
                            :hidden-parent (scene-visibility/hidden-outline-key-path? hidden-node-outline-key-paths node-outline-key-path)
                            :scene-visibility (if (contains? hidden-node-outline-key-paths node-outline-key-path)
                                                :hidden
                                                :visible))]
              {:outline outline
               :child-error (or (:child-error outline) (:outline-error? outline))
               :child-overridden (or (:child-overridden outline) (:outline-overridden? outline))}))]
    (let [outline (:outline (decorate outline [] [] (:outline-reference? outline)))
          node-id->node-id-paths (util/group-into {} [] key val
                                                  (coll/transfer [outline] :eduction
                                                    (coll/tree-xf :children :children)
                                                    (map (coll/pair-fn :node-id :node-id-path))))
          default-selection (coll/transfer selection {}
                              (keep #(when-let [node-id-paths (node-id->node-id-paths %)]
                                       (coll/pair % #{(first node-id-paths)}))))
          full-expanded (->> node-id->node-id-paths
                             (e/mapcat val)
                             (e/mapcat #(e/take-while coll/not-empty (iterate pop %)))
                             (reduce #(assoc! %1 %2 false) (transient {}))
                             persistent!)
          default-expanded (coll/merge
                             full-expanded
                             (-> default-selection
                                 auto-expand
                                 (assoc (:node-id-path outline) true)))]
      {:outline outline
       :default-selected-node-id->node-id-paths default-selection
       :default-node-id-path->expanded default-expanded})))

(def ^:private ext-with-tree-view-props
  (fx/make-ext-with-props
    (assoc fx.tree-view/props
      :selected-node-id-paths
      (fx.prop/make
        (fx.mutator/setter
          (fn set-selected-node-id-paths [^TreeView tree-view [new-selected-node-id-paths _key]]
            (let [selection-model (.getSelectionModel tree-view)
                  old-selected-node-id-paths (coll/transfer (.getSelectedItems selection-model) #{}
                                               (keep #(some-> % TreeItem/.getValue :node-id-path)))]
              (when-not (= old-selected-node-id-paths new-selected-node-id-paths)
                (let [new-selected-indices (coll/transfer (range (.getExpandedItemCount tree-view)) []
                                             (filter #(contains? new-selected-node-id-paths (:node-id-path (.getValue (.getTreeItem tree-view %))))))]
                  (.clearSelection selection-model)
                  (when-not (coll/empty? new-selected-indices)
                    (let [first-index (new-selected-indices 0)]
                      (.selectIndices selection-model (peek new-selected-indices) (into-array Integer/TYPE (pop new-selected-indices)))
                      (ui/scroll-tree-view-to-encompass-selection! tree-view)
                      (ui/run-later (.focus (.getFocusModel tree-view) first-index)))))))))
        fx.lifecycle/scalar))))

;; Iff every item-iterator has the same parent, return that parent, else nil
(defn- single-parent-it [item-iterators]
  (when (not-empty item-iterators)
    (let [parents (map outline/parent item-iterators)]
      (when (apply = parents)
        (first parents)))))

(defn- set-paste-parent! [root-its]
  (alter-var-root #'*paste-into-parent* (constantly (some-> (single-parent-it root-its) outline/value :node-id))))

(defn- update-selection! [app-view swap-state active-resource-node tree-items]
  (set-paste-parent! nil)
  ;; TODO - handle selection order
  (swap-state update :active-node-id->selected-node-id->node-id-paths
              assoc active-resource-node
              (->> tree-items
                   (e/keep #(-> ^TreeItem % .getValue))
                   (util/group-into {} #{} :node-id :node-id-path)))
  (app-view/select! app-view (coll/transfer tree-items []
                               (keep #(-> ^TreeItem % .getValue :node-id))
                               (distinct))))

(defn- outline->tree-item [{:keys [node-id-path] :as outline} node-id-path->expanded swap-state active-resource-node]
  {:fx/type fx.tree-item/lifecycle
   :value outline
   :expanded (node-id-path->expanded node-id-path)
   :on-expanded-changed #(swap-state update :active-node-id->node-id-path->expanded
                                     update active-resource-node
                                     assoc node-id-path %)
   :children (->> outline
                  :children
                  (mapv #(outline->tree-item % node-id-path->expanded swap-state active-resource-node)))})

(defn- reset-tree-view-state [old {:keys [active-resource-node selection open-resource-nodes active-node-id->selected-node-id->node-id-paths active-node-id->node-id-path->expanded]}]
  (let [ret (-> old
                (update :active-node-id->selected-node-id->node-id-paths
                        (fn [old-active-node-id->selected-node-id->node-id-paths]
                          (let [old-active-node-id->selected-node-id->node-id-paths (select-keys old-active-node-id->selected-node-id->node-id-paths open-resource-nodes)]
                            (coll/merge-with
                              (fn [old-selected-node-id->node-id-paths new-selected-node-id->node-id-paths]
                                (persistent!
                                  (reduce-kv
                                    (fn [acc new-selected-node-id _]
                                      (if-let [old-node-id-paths (old-selected-node-id->node-id-paths new-selected-node-id)]
                                        (assoc! acc new-selected-node-id old-node-id-paths)
                                        acc))
                                    (transient new-selected-node-id->node-id-paths)
                                    new-selected-node-id->node-id-paths)))
                              old-active-node-id->selected-node-id->node-id-paths
                              active-node-id->selected-node-id->node-id-paths))))
                (assoc :selection selection
                       :active-resource-node active-resource-node
                       :open-resource-nodes open-resource-nodes))
        ret (update ret :active-node-id->node-id-path->expanded
                    (fn [old-active-node-id->node-id-path->expanded]
                      (let [old-active-node-id->node-id-path->expanded (select-keys old-active-node-id->node-id-path->expanded open-resource-nodes)
                            auto-expanded (-> ret
                                              :active-node-id->selected-node-id->node-id-paths
                                              (get active-resource-node)
                                              auto-expand)]
                        (coll/merge-with
                          (fn [old-node-id-path->expanded new-node-id-path->expanded]
                            (persistent!
                              (reduce-kv
                                (fn [acc new-node-id-path _]
                                  (assoc! acc new-node-id-path (boolean (or (old-node-id-path->expanded new-node-id-path)
                                                                            (auto-expanded new-node-id-path)))))
                                (transient old-node-id-path->expanded)
                                new-node-id-path->expanded)))
                          old-active-node-id->node-id-path->expanded
                          active-node-id->node-id-path->expanded))))]
    ret))

(fxui/defc cljfx-tree-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}
             {:fx/type fxui/ext-memo
              :fn decorate-outline
              :args [(:active-outline props)
                     (:hidden-node-outline-key-paths props)
                     (:outline-name-paths props)
                     (:localization-state props)
                     (set (:selection props))]
              :key :decorated-outline}
             {:fx/type fx/ext-state
              :reset reset-tree-view-state
              :initial-state
              (let [{:keys [active-resource-node selection open-resource-nodes decorated-outline]} props
                    {:keys [default-selected-node-id->node-id-paths default-node-id-path->expanded]} decorated-outline]
                {:active-node-id->selected-node-id->node-id-paths {active-resource-node default-selected-node-id->node-id-paths}
                 :active-node-id->node-id-path->expanded {active-resource-node default-node-id-path->expanded}
                 :active-resource-node active-resource-node
                 :selection (set selection)
                 :open-resource-nodes (set open-resource-nodes)})}
             {:fx/type fx/ext-recreate-on-key-changed
              :key (:active-resource-node props)}]}
  [{:keys [tree-view app-view active-resource-node state swap-state decorated-outline]}]
  (let [{:keys [active-node-id->selected-node-id->node-id-paths active-node-id->node-id-path->expanded]} state
        {:keys [outline]} decorated-outline
        selected-node-id->node-id-paths (get active-node-id->selected-node-id->node-id-paths active-resource-node)
        node-id-path->expanded (get active-node-id->node-id-path->expanded active-resource-node)
        root (outline->tree-item outline node-id-path->expanded swap-state active-resource-node)]
    {:fx/type fx.ext.tree-view/with-selection-props
     :props {:on-selected-items-changed #(update-selection! app-view swap-state active-resource-node %)}
     :desc
     {:fx/type ext-with-tree-view-props
      :props {:root root
              :selected-node-id-paths (coll/pair
                                        (coll/transfer selected-node-id->node-id-paths #{}
                                          (mapcat val))
                                        ;; we use tree as a key, so that changes to the
                                        ;; tree without changes to selection (like
                                        ;; re-ordering) will also cause the selection to
                                        ;; sync
                                        root)}
      :desc
      {:fx/type fxui/ext-value :value tree-view}}}))

(g/defnk update-tree-view [raw-tree-view
                           selection
                           active-outline
                           hidden-node-outline-key-paths
                           outline-name-paths
                           localization
                           app-view
                           active-resource-node
                           open-resource-nodes]
  (doto raw-tree-view
    (fxui/advance-ui-user-data-component!
      ::tree
      {:fx/type cljfx-tree-view
       :tree-view raw-tree-view
       :selection selection
       :active-outline active-outline
       :hidden-node-outline-key-paths hidden-node-outline-key-paths
       :outline-name-paths outline-name-paths
       :localization localization
       :app-view app-view
       :active-resource-node active-resource-node
       :open-resource-nodes open-resource-nodes})))

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
  (property localization g/Any)

  (input app-view g/NodeID)
  (input active-outline g/Any :substitute {})
  (input active-resource-node g/NodeID :substitute nil)
  (input open-resource-nodes g/Any :substitute [])
  (input selection g/Any :substitute [])
  (input hidden-node-outline-key-paths types/NodeOutlineKeyPaths)
  (input outline-name-paths scene-visibility/OutlineNamePaths)

  (output tree-view TreeView :cached update-tree-view)
  (output tree-selection g/Any :cached (g/fnk [tree-view] (ui/selection tree-view)))
  (output tree-selection-root-its g/Any :cached (g/fnk [tree-view] (ui/selection-root-items tree-view :node-id)))
  (output succeeding-tree-selection g/Any :cached (g/fnk [tree-view tree-selection-root-its]
                                                    (ui/succeeding-selection tree-view tree-selection-root-its)))
  (output alt-tree-selection g/Any :cached (g/fnk [tree-selection]
                                                  (alt-selection tree-selection))))

(handler/register-menu! ::outline-menu
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
   {:label (localization/message "command.file.show-in-assets")
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-assets}
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
   {:label (localization/message "command.edit.add-embedded-component")
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-embedded-component
    :expand true}
   {:label (localization/message "command.edit.add-referenced-component")
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-referenced-component}
   {:label (localization/message "command.edit.add-secondary-embedded-component")
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-secondary-embedded-component}
   {:label (localization/message "command.edit.add-secondary-referenced-component")
    :icon "icons/32/Icons_M_07_plus.png"
    :command :edit.add-secondary-referenced-component}
   menu-items/separator
   {:label (localization/message "command.edit.cut")
    :command :edit.cut}
   {:label (localization/message "command.edit.copy")
    :command :edit.copy}
   {:label (localization/message "command.edit.paste")
    :command :edit.paste}
   {:label (localization/message "command.edit.delete")
    :icon "icons/32/Icons_M_06_trash.png"
    :command :edit.delete}
   menu-items/separator
   {:label (localization/message "command.edit.rename")
    :command :edit.rename}
   menu-items/separator
   {:label (localization/message "command.edit.reorder-up")
    :command :edit.reorder-up}
   {:label (localization/message "command.edit.reorder-down")
    :command :edit.reorder-down}
   menu-items/separator
   {:label (localization/message "command.scene.visibility.toggle-selection")
    :command :scene.visibility.toggle-selection}
   {:label (localization/message "command.scene.visibility.hide-unselected")
    :command :scene.visibility.hide-unselected}
   {:label (localization/message "command.scene.visibility.show-last-hidden")
    :command :scene.visibility.show-last-hidden}
   {:label (localization/message "command.scene.visibility.show-all")
    :command :scene.visibility.show-all}
   (menu-items/separator-with-id ::context-menu-end)])

(defn- root-iterators
  ([outline-view]
   (g/with-auto-evaluation-context evaluation-context
     (root-iterators outline-view evaluation-context)))
  ([outline-view evaluation-context]
   (g/node-value outline-view :tree-selection-root-its evaluation-context)))

(handler/defhandler :edit.delete :workbench
  (active? [selection evaluation-context] (handler/selection->node-ids selection evaluation-context))
  (enabled? [selection outline-view evaluation-context]
            (and (< 0 (count selection))
                 (-> (root-iterators outline-view evaluation-context)
                   outline/delete?)))
  (run [app-view selection selection-provider outline-view]
    (g/let-ec [basis (:basis evaluation-context)
               old-selected-node-ids (handler/selection->node-ids selection evaluation-context)

               deleted-node-ids
               (coll/transfer old-selected-node-ids []
                 (map #(g/override-root basis %))
                 (distinct))

               new-selected-node-ids
               (-> (handler/succeeding-selection selection-provider evaluation-context)
                   (handler/selection->node-ids evaluation-context))]
      (g/transact
        (concat
          (g/operation-label (localization/message "operation.delete"))
          (map g/delete-node deleted-node-ids)
          (when (seq new-selected-node-ids)
            (app-view/select app-view new-selected-node-ids)))))))

(def data-format-fn (fn []
                      (let [json "application/json"]
                        (or (DataFormat/lookupMimeType json)
                            (DataFormat. (into-array String [json]))))))

(handler/defhandler :edit.copy :workbench
  (active? [selection evaluation-context] (handler/selection->node-ids selection evaluation-context))
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
  (active? [selection evaluation-context] (handler/selection->node-ids selection evaluation-context))
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
  (active? [selection evaluation-context] (handler/selection->node-ids selection evaluation-context))
  (enabled? [selection outline-view evaluation-context]
            (let [item-iterators (root-iterators outline-view evaluation-context)]
              (and (< 0 (count item-iterators))
                   (outline/cut? item-iterators))))
  (run [app-view selection-provider outline-view project]
       (let [item-iterators (root-iterators outline-view)
             cb (Clipboard/getSystemClipboard)
             data-format (data-format-fn)
             next (g/with-auto-evaluation-context evaluation-context
                    (-> (handler/succeeding-selection selection-provider evaluation-context)
                        (handler/selection->node-ids evaluation-context)))]
         (.setContent cb {data-format (outline/cut! project item-iterators (if next (app-view/select app-view next)))}))))

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

(defn- find-tree-cell-ancestor [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (recur (.getParent node)))))

(defn- drag-over [project outline-view ^DragEvent e]
  (when-let [^TreeCell cell (find-tree-cell-ancestor (.getTarget e))]
    (let [db (.getDragboard e)]
      (when (and (instance? TreeCell cell)
                 (not (.isEmpty cell))
                 (.hasContent db (data-format-fn)))
        (let [item-iterators (if (ui/drag-internal? e)
                               (root-iterators outline-view)
                               [])]
          (when (outline/drop? project item-iterators (.getTreeItem cell)
                               (.getContent db (data-format-fn)))
            (let [modes (if (ui/drag-internal? e)
                          [TransferMode/MOVE]
                          [TransferMode/COPY])]
              (.acceptTransferModes e (into-array TransferMode modes)))
            (.consume e)))))))

(defn- drag-dropped [project app-view outline-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (let [item-iterators (if (ui/drag-internal? e)
                           (root-iterators outline-view)
                           [])]
      (when (outline/drop! project item-iterators (.getTreeItem cell)
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
        (when (outline/drop? project item-iterators (.getTreeItem cell)
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
  (when (ui/user-data tree-view ::editing-id)
    (set-editing-id! tree-view nil)))

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
  [^TreeView tree-view drag-entered-handler drag-exited-handler localization]
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
                     (let [{:keys [node-id label icon link color outline-error? outline-overridden? outline-reference? outline-show-link? parent-reference? child-error child-overridden scene-visibility hideable node-outline-key-path hidden-parent]} item
                           icon (if outline-error? "icons/32/Icons_E_02_error.png" icon)
                           show-link (and (some? link)
                                          (or outline-reference? outline-show-link?))
                           message (if show-link (localization/join " - " [label (resource/resource->proj-path link)]) label)
                           hidden? (= :hidden scene-visibility)
                           visibility-icon (if hidden? eye-icon-closed eye-icon-open)
                           editing? (= node-id (ui/user-data tree-view ::editing-id))]
                       (.setImage image-view-icon (icons/get-image icon))
                       (localization/localize! text-label localization message)
                       (ui/user-data! text-field ::node-id node-id)
                       (ui/user-data! text-label ::node-id node-id)
                       (doto visibility-button
                         (.setGraphic visibility-icon)
                         (ui/on-click! (partial toggle-visibility! node-outline-key-path)))
                       (proxy-super setGraphic pane)
                       (.setStyle text-label
                                  (when-let [[r g b a] color]
                                    (format "-fx-text-fill: rgba(%d, %d, %d %d);" (int (* 255 r)) (int (* 255 g)) (int (* 255 b)) (int (* 255 a)))))
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
                       (if child-error
                         (ui/add-style! this "child-error")
                         (ui/remove-style! this "child-error"))
                       (if child-overridden
                         (ui/add-style! this "child-overridden")
                         (ui/remove-style! this "child-overridden"))
                       (if hideable
                         (ui/add-style! this "hideable")
                         (ui/remove-style! this "hideable"))
                       (if hidden-parent
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

(defonce/record SelectionProvider [outline-view]
  handler/SelectionProvider
  (selection [_this evaluation-context]
    (g/node-value outline-view :tree-selection evaluation-context))
  (succeeding-selection [_this evaluation-context]
    (g/node-value outline-view :succeeding-tree-selection evaluation-context))
  (alt-selection [_this evaluation-context]
    (g/node-value outline-view :alt-tree-selection evaluation-context)))

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

(defn- setup-tree-view [project ^TreeView tree-view outline-view app-view localization]
  (let [drag-entered-handler (ui/event-handler e (drag-entered project outline-view e))
        drag-exited-handler (ui/event-handler e (drag-exited e))]
    (doto tree-view
      (.setSkin (ExtendedTreeViewSkin. tree-view))
      (ui/customize-tree-view! {:double-click-expand? true})
      (.. getSelectionModel (setSelectionMode SelectionMode/MULTIPLE))
      (.setOnDragDetected (ui/event-handler e 
                            (drag-detected project outline-view e)
                            (cancel-rename! tree-view)))
      (.setOnDragOver (ui/event-handler e (drag-over project outline-view e)))
      (.setOnDragDropped (ui/event-handler e (error-reporting/catch-all! (drag-dropped project app-view outline-view e))))
      (.setCellFactory #(make-tree-cell % drag-entered-handler drag-exited-handler localization))
      (ui/register-context-menu ::outline-menu)
      (.addEventHandler ContextMenuEvent/CONTEXT_MENU_REQUESTED (ui/event-handler event (cancel-rename! tree-view)))
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (key-pressed-handler! app-view tree-view event)))
      (.addEventFilter DragEvent/DRAG_OVER (ui/event-handler e (ui/handle-tree-view-scroll-on-drag! tree-view e)))
      (ui/context! :outline {:outline-view outline-view} (SelectionProvider. outline-view) {} {java.lang.Long :node-id
                                                                                               resource/Resource :link}))))

(defn make-outline-view [view-graph project tree-view app-view localization]
  (let [outline-view (first
                       (g/tx-nodes-added
                         (g/transact
                           (g/make-nodes view-graph [outline-view [OutlineView
                                                                   :raw-tree-view tree-view
                                                                   :localization localization]]
                             (g/connect app-view :_node-id outline-view :app-view)))))]
    (setup-tree-view project tree-view outline-view app-view localization)
    outline-view))
