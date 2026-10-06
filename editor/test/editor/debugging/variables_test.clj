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

(ns editor.debugging.variables-test
  (:require [clojure.test :refer :all]
            [editor.debugging.dap :as dap]
            [editor.debugging.variables :as variables]
            [editor.ui :as ui]
            [support.dap-util :as dap-util]
            [util.coll :as coll])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [javafx.geometry Orientation]
           [javafx.scene Scene]
           [javafx.scene.control ScrollBar TreeCell TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout StackPane]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- variable [name value reference]
  {:name name
   :value value
   :variablesReference reference})

(defn- item-at
  ^TreeItem [^TreeView view names]
  (reduce
    (fn [^TreeItem parent name]
      (when parent
        (coll/first-where #(= name (:name (.getValue ^TreeItem %))) (.getChildren parent))))
    (.getRoot view)
    names))

(defmacro ^:private with-view
  "Runs the body with a variables view, binding view, session, and events."
  [root-variables child-variables & body]
  `(let [root-variables# ~root-variables
         child-variables# ~child-variables
         ;; Keep with-adapter's port and requests bindings out of the test body.
         f# (fn [~(with-meta 'view {:tag 'javafx.scene.control.TreeView}) ~'session ~'events]
              ~@body)
         view# (ui/run-now
                 (let [view# (doto (variables/make-view!) (.setFixedCellSize 24.0))
                       pane# (doto (StackPane.) (ui/children! [view#]))]
                   (Scene. pane# 400.0 200.0)
                   (doto pane# (.resize 400.0 200.0) (.applyCss) (.layout))
                   view#))]
     (try
       (dap-util/with-adapter
         {}
         (fn [request# _# out# _socket#]
           (case (:command request#)
             "evaluate"
             (do
               (dap-util/event! out# "continued" {:threadId 7 :allThreadsContinued true})
               (dap-util/event! out# "stopped" {:threadId 7})
               (dap-util/respond! out# request# {}))

             "scopes"
             (dap-util/respond! out# request# {:scopes [{:name "Locals"
                                                         :variablesReference (+ 10000 (long (get-in request# [:arguments :frameId])))}]})

             "variables"
             (let [reference# (long (get-in request# [:arguments :variablesReference]))
                   values# (if (>= reference# 10000)
                             (root-variables# request# out# (- reference# 10000))
                             (child-variables# request# out# reference#))]
               (when values#
                 (dap-util/respond! out# request# {:variables values#})))))
         (f# view# ~'session ~'events))
       (finally (ui/run-now (variables/clear! view#))))))

(defn- pause! [view session events frame-id]
  (dap/evaluate! session nil "pause")
  (let [event (dap-util/take-event! events)
        [_ snapshot] (if (= :continued (first event)) (dap-util/take-event! events) event)]
    (ui/run-now (variables/show-frame! view session snapshot frame-id))))

;; Opened paths reload new references and values across stops; collapsed branches stay closed.
(deftest restore-expanded-paths-test
  (let [requests (atom [])]
    (with-view
      (fn [_ _ generation]
        (let [base (* (long generation) 100)]
          [(variable "self" "table" (+ base 1))
           (variable "_G" "table" (+ base 3))
           (variable "unopened" "table" (+ base 99))]))
      (fn [_ _ reference]
        (let [generation (quot (long reference) 100)
              base (* generation 100)]
          (swap! requests conj [generation reference])
          (cond
            (= reference (+ base 1)) [(variable "nested" "table" (+ base 2))
                                      (variable "cycle" "table" (+ base 1))]
            (= reference (+ base 2)) [(variable "count" (str generation) 0)]
            (= reference (+ base 3)) [(variable "global" (str generation) 0)])))
      (dap-util/await! (pause! view session events 1))
      (dap-util/await-ui! view #(item-at view ["self"]))
      (is (= [] @requests))
      (let [^TreeItem self (ui/run-now (item-at view ["self"]))]
        (ui/run-now (.setExpanded self true))
        (dap-util/await-ui! view #(item-at view ["self" "nested"])))
      (ui/run-now
        (.setExpanded (item-at view ["self" "nested"]) true)
        (.setExpanded (item-at view ["self" "cycle"]) true)
        (.setExpanded (item-at view ["_G"]) true))
      (dap-util/await-ui! view #(and (item-at view ["self" "nested" "count"])
                                     (item-at view ["self" "cycle" "cycle"])
                                     (item-at view ["_G" "global"])))
      (reset! requests [])
      (dap-util/await! (pause! view session events 2))
      (dap-util/await-ui! view #(and (item-at view ["self" "nested" "count"])
                                     (item-at view ["self" "cycle" "cycle"])
                                     (item-at view ["_G" "global"])))
      (ui/run-now
        (is (= "2" (:value (.getValue (item-at view ["self" "nested" "count"])))))
        (is (.isExpanded (item-at view ["self" "cycle"])))
        (is (not (.isExpanded (item-at view ["self" "cycle" "cycle"]))))
        (is (not (.isExpanded (item-at view ["unopened"])))))
      (is (= {[2 201] 2 [2 202] 1 [2 203] 1} (frequencies @requests)))
      (ui/run-now (.setExpanded (item-at view ["self" "nested"]) false))
      (reset! requests [])
      (dap-util/await! (pause! view session events 3))
      (dap-util/await-ui! view #(and (item-at view ["self" "cycle" "cycle"])
                                     (item-at view ["_G" "global"])))
      (ui/run-now (is (not (.isExpanded (item-at view ["self" "nested"])))))
      (is (= {[3 301] 2 [3 303] 1} (frequencies @requests))))))

;; Saved expansion survives a table becoming scalar or absent without fetching its children.
(deftest changing-variable-shapes-test
  (let [requests (atom [])]
    (with-view
      (fn [_ _ generation]
        (case (long generation)
          2 [(variable "self" "nil" 0)]
          3 []
          [(variable "self" "table" generation)]))
      (fn [_ _ reference]
        (swap! requests conj reference)
        [(variable "value" (str reference) 0)])
      (dap-util/await! (pause! view session events 1))
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "value"]))
      (dap-util/await! (pause! view session events 2))
      (dap-util/await-ui! view #(= "nil" (some-> (item-at view ["self"]) .getValue :value)))
      (dap-util/await! (pause! view session events 3))
      (ui/run-now (is (zero? (count (.getChildren (.getRoot view))))))
      (is (= [1] @requests))
      (dap-util/await! (pause! view session events 4))
      (dap-util/await-ui! view #(item-at view ["self" "value"]))
      (ui/run-now (is (= "4" (:value (.getValue (item-at view ["self" "value"]))))))
      (is (= [1 4] @requests)))))

;; A completed old frame refresh cannot overwrite another frame in the same
;; suspension, guarding against relying only on DAP stop-generation checks.
(deftest stale-frame-response-test
  (let [pending (promise)]
    (with-view
      (fn [request out frame-id]
        (if (= 1 frame-id)
          (do (deliver pending [request out]) nil)
          [(variable "new" "2" 0)]))
      (fn [_ _ _] [])
      (let [old-refresh (pause! view session events 1)
            [request out] (dap-util/await! pending)
            snapshot (dap/suspension @session)]
        (try
          (dap-util/await! (ui/run-now (variables/show-frame! view session snapshot 2)))
          (dap-util/await-ui! view #(item-at view ["new"]))
          (finally
            (dap-util/respond! out request {:variables [(variable "old" "1" 0)]})
            (dap-util/await! old-refresh)))
        (ui/run-now
          (is (= ["new"] (mapv #(-> ^TreeItem % .getValue :name)
                               (.getChildren (.getRoot view)))))
          (is (= "2" (:value (.getValue (item-at view ["new"]))))))))))

(defn- viewport [^TreeView view]
  (let [skin ^ExtendedTreeViewSkin (.getSkin view)
        flow (.getVirtualFlowInstance skin)
        cell ^TreeCell (.getFirstVisibleCell flow)
        bar ^ScrollBar (coll/first-where #(= Orientation/HORIZONTAL (.getOrientation ^ScrollBar %))
                                         (.lookupAll view ".scroll-bar"))]
    {:name (some-> cell .getTreeItem .getValue :name)
     :offset (.getLayoutY cell)
     :selection (:name (first (ui/selection view)))
     :horizontal (some-> bar .getValue)}))

;; Clearing selection with the keyboard survives a pending wire response and the next stop.
(deftest cleared-selection-survives-loading-test
  (let [pending (promise)]
    (with-view
      (fn [_ _ frame-id] [(variable "self" "table" frame-id) (variable "other" "scalar" 0)])
      (fn [request out reference]
        (if (realized? pending)
          [(variable "child" (str reference) 0)]
          (do (deliver pending [request out]) nil)))
      (dap-util/await! (pause! view session events 1))
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (let [[request out] (dap-util/await! pending)]
        (ui/run-now
          (.applyCss view)
          (.layout view)
          (let [other (item-at view ["other"])
                mac (.startsWith (System/getProperty "os.name") "Mac")]
            (.select (.getSelectionModel view) other)
            (.focus (.getFocusModel view) (.getRow view other))
            (is (= "other" (:name (first (ui/selection view)))))
            (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/SPACE false true false mac))
            (is (coll/empty? (ui/selection view)))))
        (dap-util/respond! out request {:variables [(variable "child" "value" 0)]})
        (dap-util/await-ui! view #(item-at view ["self" "child"]))
        (ui/run-now (is (coll/empty? (ui/selection view))))
        (dap-util/await! (pause! view session events 2))
        (dap-util/await-ui! view #(= "2" (some-> (item-at view ["self" "child"]) .getValue :value)))
        (ui/run-now (is (coll/empty? (ui/selection view))))))))

;; Selection and scroll positions survive refreshed rows; user navigation overrides delayed restoration.
(deftest restore-scroll-position-test
  (let [pending (promise)
        values (fn [generation]
                 (into (if (= 1 generation) [] [(variable "new sibling" "added" 0)])
                       (map #(variable (str "field-" %) (str generation) 0))
                       (range 100)))]
    (with-view
      (fn [_ _ generation] [(variable "self" "table" generation) (variable "other" (.repeat "wide " 100) 0)])
      (fn [request out reference]
        (if (= 3 reference)
          (do (deliver pending [request out]) nil)
          (values reference)))
      (dap-util/await! (pause! view session events 1))
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "field-99"]))
      (let [before
            (ui/run-now
              (.applyCss view)
              (.layout view)
              (let [flow (.getVirtualFlowInstance ^ExtendedTreeViewSkin (.getSkin view))]
                (.select (.getSelectionModel view) (item-at view ["self" "field-44"]))
                (.scrollToTop flow (int 40))
                (.layout flow)
                (.scrollPixels flow 7.0))
              (let [bar ^ScrollBar (coll/first-where #(= Orientation/HORIZONTAL (.getOrientation ^ScrollBar %))
                                                     (.lookupAll view ".scroll-bar"))]
                (.setValue bar 20.0))
              (viewport view))]
        (is (= "field-44" (:selection before)))
        (is (= 20.0 (:horizontal before)))
        (dap-util/await! (pause! view session events 2))
        (dap-util/await-ui! view #(and (= "2" (some-> (item-at view ["self" "field-99"]) .getValue :value))
                                       (item-at view ["self" "new sibling"])))
        (ui/run-now
          (let [after (viewport view)]
            (is (= (:name before) (:name after)))
            (is (= (:selection before) (:selection after)))
            (is (= (:horizontal before) (:horizontal after)))
            (is (< (Math/abs (- (double (:offset before)) (double (:offset after)))) 0.5))))
        (dap-util/await! (pause! view session events 3))
        (let [[request out] (dap-util/await! pending)]
          (dap-util/await-ui! view #(= 3 (some-> (item-at view ["self"]) .getValue :variablesReference)))
          (ui/run-now
            (.applyCss view)
            (.layout view)
            (.select (.getSelectionModel view) (item-at view ["self"]))
            (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/END false false false false))
            (.setValue ^ScrollBar (coll/first-where #(= Orientation/HORIZONTAL (.getOrientation ^ScrollBar %))
                                                    (.lookupAll view ".scroll-bar")) 40.0)
            (is (= "other" (:name (first (ui/selection view))))))
          (dap-util/respond! out request {:variables (values 3)})
          (dap-util/await-ui! view #(= "3" (some-> (item-at view ["self" "field-99"]) .getValue :value)))
          (ui/run-now
            (.layout view)
            (is (= "other" (:name (first (ui/selection view)))))
            (is (= 40.0 (:horizontal (viewport view))))
            (is (not= (:name before) (:name (viewport view))))))))))
