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

(ns editor.debugging.variables
  (:require [cljfx.api :as fx]
            [cljfx.ext.tree-view :as fx.ext.tree-view]
            [cljfx.fx.tree-item :as fx.tree-item]
            [cljfx.fx.tree-view :as fx.tree-view]
            [cljfx.lifecycle :as fx.lifecycle]
            [editor.console :as console]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [javafx.geometry Orientation]
           [javafx.scene.control ScrollBar TreeCell TreeItem TreeView]
           [javafx.scene.input KeyEvent MouseEvent ScrollEvent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private ^:dynamic *updating-view* false)

(defn- current-load? [model {:keys [session snapshot] :as context}]
  (and (identical? context (:context @model))
       (= snapshot (dap/suspension session))))

(defn- horizontal-scroll-bar
  ^ScrollBar [^TreeView view]
  (coll/first-where #(= Orientation/HORIZONTAL (.getOrientation ^ScrollBar %))
                    (.lookupAll view ".scroll-bar")))

(defn- capture-scroll-position [^TreeView view]
  (when (.getRoot view)
    (let [skin ^ExtendedTreeViewSkin (.getSkin view)
          flow (.getVirtualFlowInstance skin)]
      (when-let [^TreeCell cell (.getFirstVisibleCell flow)]
        (when-let [path (some-> cell .getTreeItem .getValue :path)]
          {:path path
           :index (.getIndex cell)
           :offset (.getLayoutY cell)
           :horizontal (some-> (horizontal-scroll-bar view) .getValue)})))))

(defn- find-item
  ^TreeItem [^TreeItem root path]
  (reduce (fn [^TreeItem parent part]
            (when parent
              (coll/first-where #(= part (peek (:path (.getValue ^TreeItem %))))
                                (.getChildren parent))))
          root
          path))

(defn- restore-scroll-position! [^TreeView view {:keys [path index offset horizontal]}]
  (.applyCss view)
  (.layout view)
  (let [skin ^ExtendedTreeViewSkin (.getSkin view)
        flow (.getVirtualFlowInstance skin)
        item (find-item (.getRoot view) path)
        row (if-not item -1 (long (.getRow view item)))]
    (.scrollToTop flow (int (if (neg? row) index row)))
    (.layout flow)
    (.scrollPixels flow (- (double offset)))
    (when horizontal
      (when-let [bar (horizontal-scroll-bar view)]
        (.setValue bar horizontal)))))

(defn- decorate-variables [parent-path variables]
  ;; Locals and upvalues can share names, so include their occurrence in the key.
  (first
    (reduce (fn [[items occurrences] {:keys [name value] :as variable}]
              (let [occurrence (long (get occurrences name 0))]
                [(conj items (assoc variable
                               :path (conj parent-path [name occurrence])
                               :display-name name
                               :display-value value))
                 (assoc occurrences name (inc occurrence))]))
            [[] {}]
            variables)))

(declare load-children!)

(defn- load-expanded! [model context]
  (doseq [[_ variables] (:children @model)
          {:keys [path variablesReference]} variables
          :when (and (pos? (long variablesReference))
                     (contains? (:expanded-paths @model) path))]
    (load-children! model context path variablesReference)))

(defn- load-children! [model {:keys [session snapshot frame-id] :as context} path reference]
  (let [{:keys [children pending]} @model]
    (when (and (current-load? model context)
               (not (contains? children path))
               (not (contains? pending path)))
      (let [completion (future/make)]
        (swap! model assoc-in [:pending path] completion)
        (-> (future/io
              (if (coll/empty? path)
                (dap/frame-variables session snapshot frame-id)
                (dap/variables session snapshot reference)))
            (future/then
              (fn [variables]
                (ui/run-later
                  (when (current-load? model context)
                    (swap! model #(-> %
                                      (assoc-in [:children path] (decorate-variables path variables))
                                      (update :pending dissoc path)))
                    ;; Restoring expansion follows only saved paths, even for cycles.
                    (load-expanded! model context))
                  (future/complete! completion nil))))
            (future/catch
              (fn [exception]
                (ui/run-later
                  (when (current-load? model context)
                    (swap! model #(-> %
                                      (assoc-in [:children path] [])
                                      (update :pending dissoc path)))
                    (console/append-console-entry! :eval-error (ex-message exception)))
                  (future/fail! completion exception)))))
        completion))))

(defn- variable-item [model {:keys [children expanded-paths context] :as state} {:keys [path variablesReference] :as variable}]
  (let [table (pos? (long variablesReference))]
    {:fx/type fx.tree-item/lifecycle
     :fx/key path
     :value variable
     :expanded (and table (contains? expanded-paths path))
     :on-expanded-changed
     (fn [expanded]
       (when (current-load? model context)
         (swap! model update :expanded-paths (if expanded conj disj) path)
         (when expanded
           (load-children! model context path variablesReference))))

     :children
     (if-not table
       []
       (let [variables (get children path)]
         (if-not variables
           [{:fx/type fx.tree-item/lifecycle :fx/key ::loading}]
           (mapv #(variable-item model state %) variables))))}))

(defn- sync-viewport! [^TreeView view model]
  (let [{:keys [context pending selection scroll-to-restore]} @model]
    (when (and context (coll/empty? pending))
      (when-let [item (when selection (find-item (.getRoot view) selection))]
        (.select (.getSelectionModel view) item))
      (when scroll-to-restore
        ;; Run after cljfx has installed every expanded branch, and recheck input.
        (ui/run-later
          (when (and (current-load? model context) (coll/empty? (:pending @model)))
            (when-let [position (:scroll-to-restore @model)]
              (swap! model dissoc :scroll-to-restore)
              (restore-scroll-position! view position))))))))

(defn- install-view! [^TreeView view model]
  (.setSkin view (ExtendedTreeViewSkin. view))
  (ui/customize-tree-view! view {:double-click-expand true})
  (ui/user-data! view ::state model)
  (let [cancel-scroll-restore
        (ui/event-handler _
          (swap! model dissoc :scroll-to-restore))]
    (.addEventFilter view ScrollEvent/SCROLL cancel-scroll-restore)
    (.addEventFilter view MouseEvent/MOUSE_PRESSED cancel-scroll-restore)
    (.addEventFilter view KeyEvent/KEY_PRESSED cancel-scroll-restore)
    (ui/user-data! view ::cancel-scroll-restore cancel-scroll-restore)))

(defn- uninstall-view! [^TreeView view]
  (let [^javafx.event.EventHandler handler (ui/user-data view ::cancel-scroll-restore)]
    (.removeEventFilter view ScrollEvent/SCROLL handler)
    (.removeEventFilter view MouseEvent/MOUSE_PRESSED handler)
    (.removeEventFilter view KeyEvent/KEY_PRESSED handler)))

(def ^:private ext-viewport
  (reify fx.lifecycle/Lifecycle
    (create [_ {:keys [desc model]} opts]
      (binding [*updating-view* true]
        (let [component (fx.lifecycle/create fx.lifecycle/dynamic desc opts)]
          (install-view! (fx/instance component) model)
          component)))
    (advance [_ component {:keys [desc model]} opts]
      (binding [*updating-view* true]
        (let [component (fx.lifecycle/advance fx.lifecycle/dynamic component desc opts)]
          (sync-viewport! (fx/instance component) model)
          component)))
    (delete [_ component opts]
      (let [view (fx/instance component)]
        (swap! (ui/user-data view ::state) assoc :context nil)
        (uninstall-view! view))
      (fx.lifecycle/delete fx.lifecycle/dynamic component opts))))

(ui/defc variables-view
  {:compose [{:fx/type fx/ext-watcher :ref (:model props) :key :state}]}
  [{:keys [model state]}]
  {:fx/type ext-viewport
   :model model
   :desc {:fx/type fx.ext.tree-view/with-selection-props
          :props {:on-selected-item-changed
                  (fn [^TreeItem item]
                    ;; Rebuilding the tree can temporarily clear selection.
                    (when-not *updating-view*
                      (swap! model assoc :selection (some-> item .getValue :path))))}
          :desc {:fx/type fx.tree-view/lifecycle
                 :id "debugger-variables"
                 :show-root false
                 :root
                 (if-not (:context state)
                   {:fx/type ui/ext-value :value nil}
                   {:fx/type fx.tree-item/lifecycle
                    :expanded true
                    :children (mapv #(variable-item model state %) (get-in state [:children []]))})}}})

(defn make-view!
  ^TreeView []
  (let [model (atom {:expanded-paths #{} :children {} :pending {}})
        component (fx/create-component {:fx/type variables-view :model model})
        view (fx/instance component)]
    (ui/user-data! view ::component component)
    view))

(defn clear!
  "Clear stale values while retaining expansion paths, selection, and viewport."
  [^TreeView view]
  (let [model (ui/user-data view ::state)
        position (or (:scroll-to-restore @model) (capture-scroll-position view) (:scroll-position @model))]
    (swap! model assoc :context nil :children {} :pending {} :scroll-position position)))

(defn show-frame!
  "Refresh a frame, restoring opened paths with new DAP references and values."
  [^TreeView view session snapshot frame-id]
  (clear! view)
  (when (and frame-id snapshot)
    (let [model (ui/user-data view ::state)
          context {:session session :snapshot snapshot :frame-id frame-id}]
      (swap! model assoc :context context :scroll-to-restore (:scroll-position @model))
      (load-children! model context [] nil))))
