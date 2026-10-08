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
            [cljfx.mutator :as fx.mutator]
            [editor.console :as console]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [javafx.event Event]
           [javafx.scene.control TreeCell TreeItem TreeView]
           [javafx.scene.input KeyEvent MouseEvent ScrollEvent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- current-load? [state {:keys [session snapshot] :as context}]
  (and (identical? context (:context state))
       (= snapshot (dap/suspension @session))))

(defn- capture-scroll-row [^TreeView view]
  (when (.getRoot view)
    (let [skin ^ExtendedTreeViewSkin (.getSkin view)
          flow (.getVirtualFlowInstance skin)]
      (when-let [^TreeCell cell (.getFirstVisibleCell flow)]
        (.getIndex cell)))))

(defn- find-item
  ^TreeItem [^TreeItem root path]
  (reduce (fn [^TreeItem parent part]
            (when parent
              (coll/first-where #(= part (peek (:path (.getValue ^TreeItem %))))
                                (.getChildren parent))))
          root
          path))

(defn- decorate-variables [parent-path variables]
  ;; Locals and upvalues can share names, so include their occurrence in the key.
  (first
    (reduce (fn [[items occurrences] {:keys [name value] :as variable}]
              (let [occurrence (long (get occurrences name 0))]
                (coll/pair (conj items (assoc variable
                                         :path (conj parent-path [name occurrence])
                                         :display-name name
                                         :display-value value))
                           (assoc occurrences name (inc occurrence)))))
            (coll/pair [] {})
            variables)))

(defn- load-children! [swap-state {:keys [session snapshot frame-id] :as context} path reference]
  ;; Claim the path in a retryable update, then start IO only for that request.
  (let [request (Object.)
        state (swap-state
                (fn [{:keys [children pending failed-paths expanded-paths] :as state}]
                  (if (and (current-load? state context)
                           (or (coll/empty? path) (contains? expanded-paths path))
                           (not (contains? children path))
                           (not (contains? pending path))
                           (not (contains? failed-paths path)))
                    (assoc-in state [:pending path] request)
                    state)))]
    (when (identical? request (get-in state [:pending path]))
      (future/io
        (try
          (let [variables (if (coll/empty? path)
                            (dap/frame-variables session snapshot frame-id)
                            (dap/variables session snapshot reference))
                {:keys [expanded-paths] :as state}
                (swap-state
                  (fn [state]
                    (if-not (current-load? state context)
                      state
                      (-> state
                          (assoc-in [:children path] (decorate-variables path variables))
                          (update :failed-paths disj path)))))]
            (when (current-load? state context)
              ;; Restoring expansion follows only saved paths, even for cycles.
              (doseq [{:keys [path variablesReference]} (get-in state [:children path])
                      :when (and (pos? (long variablesReference))
                                 (contains? expanded-paths path))]
                (load-children! swap-state context path variablesReference)))
            ;; Keep the parent pending until restored descendants have claimed their loads.
            (swap-state
              (fn [state]
                (if-not (current-load? state context)
                  state
                  (update state :pending dissoc path)))))
          (catch Exception exception
            (let [state (swap-state
                          (fn [state]
                            (if-not (current-load? state context)
                              state
                              ;; Keep failures expandable without automatically retrying them.
                              (-> state
                                  (update :failed-paths conj path)
                                  (update :pending dissoc path)))))]
              (when (current-load? state context)
                (console/append-console-entry! :eval-error (ex-message exception))))))))))

(defn- variable-at [state path]
  (coll/first-where #(= path (:path %)) (get (:children state) (pop path))))

(defn- variable-item [swap-state {:keys [children expanded-paths] :as state} {:keys [path variablesReference] :as variable}]
  (let [table (pos? (long variablesReference))]
    {:fx/type fx.tree-item/lifecycle
     :fx/key path
     :value variable
     :expanded (and table (contains? expanded-paths path))

     :on-expanded-changed
     (fn [expanded]
       (let [{:keys [context] :as state}
             (swap-state
               (fn [{:keys [context] :as state}]
                 (let [reference (:variablesReference (variable-at state path))]
                   (if-not (and context reference (pos? (long reference)) (current-load? state context))
                     state
                     (cond-> (update state :expanded-paths (if expanded conj disj) path)
                       expanded (update :failed-paths disj path))))))
             reference (:variablesReference (variable-at state path))]
         (when (and expanded context reference (pos? (long reference)))
           (load-children! swap-state context path reference))))

     :children
     (if-not table
       []
       (let [variables (get children path)]
         (if-not variables
           [{:fx/type fx.tree-item/lifecycle :fx/key ::loading}]
           (mapv #(variable-item swap-state state %) variables))))}))

(def ^:private prop-restored-selection
  (fx/make-prop
    (fx.mutator/setter
      (fn [^TreeView view selection]
        (let [selection-model (.getSelectionModel view)]
          (when-let [item (when selection (find-item (.getRoot view) selection))]
            (when-not (identical? item (.getSelectedItem selection-model))
              (.select selection-model item))))))
    fx.lifecycle/scalar))

(def ^:private prop-restored-scroll-row
  (fx/make-prop
    (fx.mutator/setter
      (fn [^TreeView view [swap-state scroll-row]]
        (when scroll-row
          (.scrollTo view (int (min (long scroll-row) (max 0 (dec (.getExpandedItemCount view))))))
          (swap-state dissoc :scroll-row))))
    fx.lifecycle/scalar))

(def ^:private prop-extended-tree-view-skin
  (fx/make-prop
    (fx.mutator/setter
      (fn [^TreeView view enabled]
        (.setSkin view (when enabled (ExtendedTreeViewSkin. view)))))
    fx.lifecycle/scalar))

(def ^:private initial-state
  {:session nil
   :expanded-paths #{}
   :children {}
   :failed-paths #{}
   :pending {}
   :scroll-row 0})

(ui/defc variables-view
  {:compose [{:fx/type fx/ext-state
              :initial-state initial-state}
             {:fx/type fx/ext-watcher
              :ref (get-in props [:state :context :session])
              :key :session-state}
             {:fx/type fx.ext.tree-view/with-selection-props
              :props
              (let [{:keys [context children pending selection scroll-row]} (:state props)
                    ready (and context
                               (= (:snapshot context) (dap/suspension (:session-state props)))
                               (contains? children [])
                               (coll/empty? pending))]
                {prop-restored-selection (when ready selection)
                 prop-restored-scroll-row [(:swap-state props) (when ready scroll-row)]
                 :on-selected-item-changed
                 (fn [^TreeItem item]
                   ((:swap-state props) assoc :selection (some-> item .getValue :path)))})}]}
  [{:keys [state swap-state]}]
  {:fx/type fx.tree-view/lifecycle
   :id "debugger-variables"
   :show-root false
   :user-data {::swap-state swap-state}
   prop-extended-tree-view-skin true

   :event-filter
   (fn [^Event event]
     (let [event-type (.getEventType event)]
       (when (contains? #{ScrollEvent/SCROLL MouseEvent/MOUSE_PRESSED KeyEvent/KEY_PRESSED} event-type)
         (swap-state dissoc :scroll-row))
       (cond
         (= KeyEvent/KEY_PRESSED event-type) (ui/custom-tree-view-key-pressed! event)
         (= MouseEvent/MOUSE_PRESSED event-type) (ui/custom-tree-view-mouse-pressed! event))))

   :root
   {:fx/type fx/ext-recreate-on-key-changed
    :key (:session state)
    :desc (if-not (:context state)
            {:fx/type ui/ext-value :value nil}
            {:fx/type fx.tree-item/lifecycle
             :expanded true
             :children (mapv #(variable-item swap-state state %) (get-in state [:children []]))})}})

(defn make-view!
  ^TreeView []
  (let [component (fx/create-component {:fx/type variables-view})
        view (fx/instance component)]
    (ui/user-data! view ::component component)
    view))

(defn clear!
  "Clear stale values while retaining tree state within the supplied session."
  [^TreeView view session]
  (let [swap-state (ui/user-data view ::swap-state)
        row (capture-scroll-row view)]
    (swap-state
      (fn [state]
        (if-not (= session (:session state))
          (assoc initial-state :session session)
          (assoc state
            :context nil
            :children {}
            :failed-paths #{}
            :pending {}
            :scroll-row (or (:scroll-row state) row)))))))

(defn show-frame!
  "Refresh a frame, restoring opened paths with new DAP references and values.

  Retain tree state within a session; start a new session collapsed at the top."
  [^TreeView view session snapshot frame-id]
  (clear! view session)
  (when (and snapshot frame-id)
    (let [swap-state (ui/user-data view ::swap-state)
          state (swap-state assoc :context {:session session :snapshot snapshot :frame-id frame-id})]
      (load-children! swap-state (:context state) [] nil))))
