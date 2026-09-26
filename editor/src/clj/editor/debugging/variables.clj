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
  (:require [editor.console :as console]
            [editor.debugging.dap :as dap]
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [java.util Collection]
           [javafx.geometry Orientation]
           [javafx.scene.control ScrollBar TreeCell TreeItem TreeView]
           [javafx.scene.input KeyEvent MouseEvent ScrollEvent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- current-load? [{:keys [^TreeView view root session snapshot]}]
  (and (identical? root (.getRoot view))
       (= snapshot (dap/suspension session))))

(defn- view-state [^TreeView view]
  (or (ui/user-data view ::state)
      (let [state (atom {:expanded-paths #{}})
            cancel-scroll-restore (ui/event-handler _
                                    (when-let [context (ui/user-data view ::context)]
                                      (vreset! (:scroll-to-restore context) nil)))]
        (ui/user-data! view ::state state)
        ;; A user scrolling or navigating during a slow refresh takes precedence.
        (.addEventFilter view ScrollEvent/SCROLL cancel-scroll-restore)
        (.addEventFilter view MouseEvent/MOUSE_PRESSED cancel-scroll-restore)
        (.addEventFilter view KeyEvent/KEY_PRESSED cancel-scroll-restore)
        state)))

(defn- horizontal-scroll-bar
  ^ScrollBar [^TreeView view]
  (coll/first-where #(= Orientation/HORIZONTAL (.getOrientation ^ScrollBar %))
                    (.lookupAll view ".scroll-bar")))

(defn- capture-scroll-position [^TreeView view]
  (let [skin (.getSkin view)]
    (when (and (.getRoot view) (instance? ExtendedTreeViewSkin skin))
      (let [flow (.getVirtualFlowInstance ^ExtendedTreeViewSkin skin)]
        (when-let [^TreeCell cell (.getFirstVisibleCell flow)]
          (when-let [path (some-> cell .getTreeItem .getValue :path)]
            {:path path
             :index (.getIndex cell)
             :offset (.getLayoutY cell)
             :horizontal (some-> (horizontal-scroll-bar view) .getValue)
             :selection (:path (first (ui/selection view)))}))))))

(defn- find-item
  ^TreeItem [^TreeItem root path]
  (reduce (fn [^TreeItem parent part]
            (when parent
              (coll/first-where #(= part (peek (:path (.getValue ^TreeItem %))))
                                (.getChildren parent))))
          root
          path))

(defn- restore-scroll-position! [^TreeView view {:keys [path index offset horizontal selection]}]
  (.applyCss view)
  (.layout view)
  (let [skin (.getSkin view)]
    (when (instance? ExtendedTreeViewSkin skin)
      (let [flow (.getVirtualFlowInstance ^ExtendedTreeViewSkin skin)
            item (find-item (.getRoot view) path)
            row (if-not item -1 (long (.getRow view item)))]
        (when-let [selected-item (when selection (find-item (.getRoot view) selection))]
          (.select (.getSelectionModel view) selected-item))
        (.scrollToTop flow (int (if (neg? row) index row)))
        (.layout flow)
        (.scrollPixels flow (- (double offset)))
        (when horizontal
          (when-let [bar (horizontal-scroll-bar view)]
            (.setValue bar horizontal)))))))

(defn clear!
  "Clear stale values while retaining expansion paths and the last viewport."
  [^TreeView view]
  (let [state (view-state view)
        context (ui/user-data view ::context)]
    ;; Do not replace the saved viewport with an intermediate, partly loaded tree.
    (when-not (and context @(:scroll-to-restore context))
      (when-let [position (capture-scroll-position view)]
        (swap! state assoc :scroll-position position)))
    (ui/user-data! view ::context nil)
    (.setRoot view nil)))

(declare load-children!)

(defn- make-items [{:keys [state session snapshot] :as context} parent-path variables]
  ;; Repeated names in locals/upvalues get distinct paths. Table key names from
  ;; DAP already distinguish string keys from numeric and other key types.
  (first
    (reduce
      (fn [[items occurrences] {:keys [name value variablesReference] :as variable}]
        (let [occurrence (long (get occurrences name 0))
              path (conj parent-path [name occurrence])
              item (TreeItem. (assoc variable
                                :path path
                                :display-name name
                                :display-value value))
              loaded (volatile! false)]
          (when (pos? (long variablesReference))
            (.add (.getChildren item) (TreeItem.))
            (ui/observe (.expandedProperty item)
                        (fn [_ _ expanded]
                          (when (current-load? context)
                            (swap! state update :expanded-paths (if expanded conj disj) path)
                            (when (and expanded (not @loaded))
                              (vreset! loaded true)
                              (load-children! context item path
                                              #(dap/variables session snapshot variablesReference))))))
            ;; This follows only previously expanded paths, even for cyclic tables.
            (when (contains? (:expanded-paths @state) path)
              (.setExpanded item true)))
          [(conj items item) (assoc occurrences name (inc occurrence))]))
      [[] {}]
      variables)))

(defn- load-children! [{:keys [pending scroll-to-restore ^TreeView view] :as context} ^TreeItem parent path fetch]
  (vswap! pending #(inc (long %)))
  (future/io
    (let [result (try
                   (fetch)
                   (catch Exception exception
                     exception))]
      (ui/run-later
        (try
          (when (current-load? context)
            (if (instance? Exception result)
              (console/append-console-entry! :eval-error (ex-message result))
              (.setAll (.getChildren parent) ^Collection (make-items context path result))))
          (finally
            (when (and (zero? (long (vswap! pending #(dec (long %))))) @scroll-to-restore)
              (ui/run-later
                (when (and (current-load? context) (zero? (long @pending)))
                  (when-let [position @scroll-to-restore]
                    (vreset! scroll-to-restore nil)
                    (restore-scroll-position! view position)))))))))))

(defn show-frame!
  "Refresh a frame, restoring opened paths with new DAP references and values."
  [^TreeView view session snapshot frame-id]
  (clear! view)
  (when (and frame-id snapshot)
    (let [state (view-state view)
          root (TreeItem.)
          context {:view view
                   :root root
                   :session session
                   :snapshot snapshot
                   :state state
                   :pending (volatile! 0)
                   :scroll-to-restore (volatile! (:scroll-position @state))}]
      (.setRoot view root)
      (ui/user-data! view ::context context)
      (load-children! context root [] #(dap/frame-variables session snapshot frame-id)))))
