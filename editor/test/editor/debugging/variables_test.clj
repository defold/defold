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
            [editor.future :as future]
            [editor.ui :as ui]
            [util.coll :as coll])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [javafx.geometry Orientation]
           [javafx.scene Scene]
           [javafx.scene.control ScrollBar TreeCell TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout StackPane]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- await-ui! [f]
  (let [deadline (+ (System/nanoTime) 10000000000)]
    (loop []
      (if-let [result (ui/run-now (f))]
        result
        (if (> (System/nanoTime) deadline)
          (throw (IllegalStateException. "Timed out waiting for debugger variables"))
          (do
            (Thread/sleep 10)
            (recur)))))))

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

(defn- pause! [view session generation frame-id]
  (ui/run-now
    (swap! (:state session) assoc :status :running)
    (variables/clear! view)
    (swap! (:state session) assoc :status :suspended :generation generation)
    (variables/show-frame! view session (dap/suspension session) frame-id)))

;; Verify opened paths reload fresh values/references across stops, unopened and
;; collapsed branches stay closed, and cycles reopen only to the saved depth.
(deftest restore-expanded-paths-test
  (let [view (ui/run-now (variables/make-view!))
        session
        {:state (atom {:status :suspended
                       :generation 1
                       :thread-id 7})}
        requests (atom [])]
    (with-redefs [dap/frame-variables
                  (fn [_ {:keys [generation]} _]
                    (let [base (* (long generation) 100)]
                      [(variable "self" "table" (+ base 1))
                       (variable "_G" "table" (+ base 3))
                       (variable "unopened" "table" (+ base 99))]))
                  dap/variables
                  (fn [_ {:keys [generation]} reference]
                    (swap! requests conj [generation reference])
                    (let [base (* (long generation) 100)]
                      (cond
                        (= reference (+ base 1))
                        [(variable "nested" "table" (+ base 2))
                         (variable "cycle" "table" (+ base 1))]

                        (= reference (+ base 2))
                        [(variable "count" (str generation) 0)]

                        (= reference (+ base 3))
                        [(variable "global" (str generation) 0)]

                        :else
                        (throw (IllegalArgumentException. (str "Unexpected or expired table reference: " reference))))))]
      (pause! view session 1 42)
      (await-ui! #(item-at view ["self"]))
      (is (= [] @requests))
      (let [^TreeItem self (ui/run-now (item-at view ["self"]))]
        (ui/run-now (.setExpanded self true))
        (await-ui! #(item-at view ["self" "nested"]))
        (is (identical? self (ui/run-now (item-at view ["self"])))))
      (ui/run-now
        (.setExpanded (item-at view ["self" "nested"]) true)
        (.setExpanded (item-at view ["self" "cycle"]) true)
        (.setExpanded (item-at view ["_G"]) true))
      (await-ui! #(and (item-at view ["self" "nested" "count"])
                       (item-at view ["self" "cycle" "cycle"])
                       (item-at view ["_G" "global"])))
      (reset! requests [])

      (testing "New frame and table IDs retain opened paths and fetch fresh values"
        (pause! view session 2 99)
        (await-ui! #(and (item-at view ["self" "nested" "count"])
                         (item-at view ["self" "cycle" "cycle"])
                         (item-at view ["_G" "global"])))
        (ui/run-now
          (is (= "2" (:value (.getValue (item-at view ["self" "nested" "count"])))))
          (is (.isExpanded (item-at view ["self" "cycle"])))
          (is (false? (.isExpanded (item-at view ["self" "cycle" "cycle"]))))
          (is (false? (.isExpanded (item-at view ["unopened"])))))
        (is (= {[2 201] 2 [2 202] 1 [2 203] 1} (frequencies @requests))))

      (testing "An explicitly collapsed branch stays collapsed at the next breakpoint"
        (ui/run-now (.setExpanded (item-at view ["self" "nested"]) false))
        (reset! requests [])
        (pause! view session 3 111)
        (await-ui! #(and (item-at view ["self" "cycle" "cycle"])
                         (item-at view ["_G" "global"])))
        (ui/run-now
          (is (false? (.isExpanded (item-at view ["self" "nested"])))))
        (is (= {[3 301] 2 [3 303] 1} (frequencies @requests))))
      (ui/run-now (variables/clear! view)))))

;; Verify a saved path survives a variable becoming scalar or disappearing,
;; without fetching children until a table with that name returns.
(deftest changing-variable-shapes-test
  (let [view (ui/run-now (variables/make-view!))
        session
        {:state (atom {:status :suspended
                       :generation 1
                       :thread-id 7})}
        requests (atom [])]
    (with-redefs [dap/frame-variables
                  (fn [_ {:keys [generation]} _]
                    (case (long generation)
                      2 [(variable "self" "nil" 0)]
                      3 []
                      [(variable "self" "table" generation)]))
                  dap/variables
                  (fn [_ {:keys [generation]} reference]
                    (swap! requests conj [generation reference])
                    [(variable "value" (str generation) 0)])]
      (pause! view session 1 42)
      (await-ui! #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (await-ui! #(item-at view ["self" "value"]))

      (testing "A scalar or absent variable does not fetch children"
        (doseq [generation [2 3]]
          (pause! view session generation 42)
          (await-ui! #(coll/empty? (:pending @(ui/user-data view :editor.debugging.variables/state)))))
        (is (= [[1 1]] @requests)))

      (testing "The path reopens if a table with that name appears again"
        (pause! view session 4 99)
        (await-ui! #(item-at view ["self" "value"]))
        (ui/run-now
          (is (= "4" (:value (.getValue (item-at view ["self" "value"]))))))
        (is (= [[1 1] [4 4]] @requests)))
      (ui/run-now (variables/clear! view)))))

;; Verify a table response from an earlier stop cannot overwrite the refreshed
;; tree, even when that old request completes after the new one.
(deftest stale-table-response-test
  (let [view (ui/run-now (variables/make-view!))
        session
        {:state (atom {:status :suspended
                       :generation 1
                       :thread-id 7})}
        started (promise)
        response (promise)]
    (with-redefs [dap/frame-variables
                  (fn [_ {:keys [generation]} _]
                    [(variable "self" "table" generation)])
                  dap/variables
                  (fn [_ _ reference]
                    (if-not (= 1 reference)
                      [(variable "new" "2" 0)]
                      (do
                        (deliver started true)
                        @response)))]
      (try
        (pause! view session 1 42)
        (await-ui! #(item-at view ["self"]))
        (let [old-load
              (ui/run-now
                (let [item (item-at view ["self"])]
                  (.setExpanded item true)
                  (get-in @(ui/user-data view :editor.debugging.variables/state)
                          [:pending (:path (.getValue item))])))]
          (is (= true (deref started 10000 ::timeout)))
          (pause! view session 2 99)
          (await-ui! #(item-at view ["self" "new"]))
          (deliver response [(variable "old" "1" 0)])
          (is (nil? (deref old-load 10000 ::timeout)))
          (ui/run-now
            (is (nil? (item-at view ["self" "old"])))
            (is (= "2" (:value (.getValue (item-at view ["self" "new"])))))))
        (finally
          (deliver response [])
          (ui/run-now (variables/clear! view)))))))

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

;; Clearing selection with the keyboard must survive a pending table response
;; and the next pause.
(deftest cleared-selection-survives-loading-test
  (let [[^TreeView view ^Stage stage]
        (ui/run-now
          (let [view (variables/make-view!)
                pane (doto (StackPane.)
                       (ui/children! [view]))
                stage (doto (Stage.)
                        (.setScene (Scene. pane 400.0 200.0))
                        (.show))]
            [view stage]))
        session {:state (atom {:status :suspended :generation 1 :thread-id 7})}
        started (future/make)
        response (future/make)]
    (with-redefs [dap/frame-variables
                  (fn [_ _ _]
                    [(variable "self" "table" 1)
                     (variable "other" "scalar" 0)])

                  dap/variables
                  (fn [_ _ _]
                    (future/complete! started true)
                    @response)]
      (try
        (pause! view session 1 42)
        (await-ui! #(item-at view ["self"]))
        (ui/run-now (.setExpanded (item-at view ["self"]) true))
        (is (= true (deref started 10000 ::timeout)))
        (ui/run-now
          (.applyCss view)
          (.layout view)
          (let [other (item-at view ["other"])
                mac (.startsWith (System/getProperty "os.name") "Mac")]
            (.select (.getSelectionModel view) other)
            (.focus (.getFocusModel view) (.getRow view other))
            (is (= "other" (:name (first (ui/selection view)))))
            ;; JavaFX uses Ctrl+Space, with Command also held on macOS.
            (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/SPACE false true false mac))
            (is (coll/empty? (ui/selection view)))))
        (future/complete! response [(variable "child" "value" 0)])
        (await-ui! #(item-at view ["self" "child"]))
        (ui/run-now
          (is (coll/empty? (ui/selection view))))
        (pause! view session 2 99)
        (await-ui! #(and (item-at view ["self" "child"])
                         (coll/empty? (:pending @(ui/user-data view :editor.debugging.variables/state)))))
        (ui/run-now
          (is (coll/empty? (ui/selection view))))
        (finally
          (future/complete! response [])
          (ui/run-now
            (variables/clear! view)
            (.close stage)))))))

;; Verify selection and both scroll positions survive changed table rows, and
;; user navigation takes precedence over a delayed viewport restoration.
(deftest restore-scroll-position-test
  (let [[^TreeView view ^Stage stage]
        (ui/run-now
          (let [view (doto (variables/make-view!)
                       (.setShowRoot false)
                       (.setFixedCellSize 24.0))
                pane (doto (StackPane.)
                       (ui/children! [view]))
                stage (doto (Stage.)
                        (.setScene (Scene. pane 400.0 200.0))
                        (.show))]
            [view stage]))
        session
        {:state (atom {:status :suspended
                       :generation 1
                       :thread-id 7})}
        started (promise)
        response (promise)]
    (with-redefs [dap/frame-variables
                  (fn [_ {:keys [generation]} _]
                    [(variable "self" "table" generation)
                     (variable "other" "scalar" 0)])
                  dap/variables
                  (fn [_ {:keys [generation]} _]
                    (when (= 3 generation)
                      (deliver started true)
                      @response)
                    (into (if (= 1 generation) [] [(variable "new sibling" "added" 0)])
                          (map #(variable (str "field-" %) (str generation) 0))
                          (range 100)))]
      (try
        (pause! view session 1 42)
        (await-ui! #(item-at view ["self"]))
        (ui/run-now (.setExpanded (item-at view ["self"]) true))
        (await-ui! #(item-at view ["self" "field-99"]))
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
          (is (string? (:name before)))
          (is (= "field-44" (:selection before)))
          (is (= 20.0 (:horizontal before)))
          (pause! view session 2 99)
          (await-ui! #(and (item-at view ["self" "field-99"])
                           (nil? (:scroll-to-restore @(ui/user-data view :editor.debugging.variables/state)))))
          (ui/run-now
            (let [after (viewport view)]
              (is (= (:name before) (:name after)))
              (is (= (:selection before) (:selection after)))
              (is (= (:horizontal before) (:horizontal after)))
              (is (< (Math/abs (- (double (:offset before)) (double (:offset after)))) 0.5))))

          (testing "User navigation takes precedence over a delayed scroll restoration"
            (pause! view session 3 111)
            (is (= true (deref started 10000 ::timeout)))
            (await-ui! #(= 3 (some-> (item-at view ["self"]) .getValue :variablesReference)))
            (ui/run-now
              (.applyCss view)
              (.layout view)
              (.select (.getSelectionModel view) (item-at view ["self"]))
              (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/END false false false false))
              (is (= "other" (:name (first (ui/selection view))))))
            (deliver response true)
            (await-ui! #(and (item-at view ["self" "field-99"])
                             (coll/empty? (:pending @(ui/user-data view :editor.debugging.variables/state)))))
            (ui/run-now
              (.layout view)
              (is (= "other" (:name (first (ui/selection view))))))))
        (finally
          (deliver response true)
          (ui/run-now
            (variables/clear! view)
            (.close stage)))))))
