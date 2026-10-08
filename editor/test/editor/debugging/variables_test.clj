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
  (:require [cljfx.api :as fx]
            [cljfx.plorer :as plorer]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.console :as console]
            [editor.debugging.dap :as dap]
            [editor.debugging.variables :as variables]
            [editor.future :as future]
            [editor.ui :as ui]
            [support.dap-util :as dap-util]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.http-server.types :as http-server.types])
  (:import [com.defold.control ExtendedTreeViewSkin]
           [java.io ByteArrayOutputStream]
           [java.nio.charset StandardCharsets]
           [java.util.concurrent LinkedBlockingQueue]
           [javafx.scene Scene]
           [javafx.scene.control TreeCell TreeItem TreeView]
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
  "Runs the body with a variables component, binding view, session, and events."
  [root-variables child-variables & body]
  `(let [root-variables# ~root-variables
         child-variables# ~child-variables
         component# (ui/run-now (fx/create-component {:fx/type variables/variables-view}))
         view# (ui/run-now
                 (let [view# (doto ^TreeView (fx/instance component#) (.setFixedCellSize 24.0))
                       pane# (doto (StackPane.) (ui/children! [view#]))]
                   (doto (Scene. pane# 400.0 200.0) (.snapshot nil))
                   view#))]
     (try
       (dap-util/with-adapter
         "/project"
         {}
         (fn [request# _# out# _socket#]
           (case (:command request#)
             "evaluate"
             (do
               (dap-util/event! out# "continued" {:threadId 7 :allThreadsContinued true})
               (dap-util/event! out# "stopped" {:threadId 7})
               (dap-util/respond! out# request# {}))

             "continue"
             (do
               (dap-util/event! out# "continued" {:threadId 7 :allThreadsContinued true})
               (dap-util/respond! out# request# {}))

             "setBreakpoints"
             (dap-util/respond! out# request# {:breakpoints []})

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
         (let [~(with-meta 'view {:tag 'javafx.scene.control.TreeView}) view#]
           ~@body))
       (finally
         (ui/run-now
           (fx/delete-component component#))))))

(defn- pause! [view session events frame-id]
  (dap/evaluate! session nil "pause")
  (let [event (dap-util/take-event! events)
        [_ snapshot] (if (= :continued (first event)) (dap-util/take-event! events) event)]
    (ui/run-now (variables/show-frame! view session snapshot frame-id))))

;; Session notifications preserve the control and loaded rows; replacement displays the new session's values.
(deftest session-subscription-lifecycle-test
  (let [loads (atom 0)]
    (with-view
      (fn [_ _ _] (swap! loads inc) [(variable "old" "value" 0)])
      (fn [_ _ _] [])
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["old"]))
      (let [root (ui/run-now (.getRoot view))
            item (ui/run-now (item-at view ["old"]))]
        (dap/set-breakpoints! session {"/main.lua" [{:line 1}]})
        (ui/run-now
          (is (identical? root (.getRoot view)))
          (is (identical? item (item-at view ["old"])))
          (is (= 1 @loads))
          (variables/clear! view nil))
        (dap-util/await-ui! view #(nil? (.getRoot view))))
      (ui/run-now (variables/show-frame! view session (dap/suspension @session) 1))
      (dap-util/await-ui! view #(item-at view ["old"]))
      (let [old-session session]
        (dap-util/with-adapter
          "/project" {}
          (fn [request _ out _]
            (case (:command request)
              "evaluate" (do (dap-util/event! out "stopped" {:threadId 7})
                             (dap-util/respond! out request {}))
              "scopes" (dap-util/respond! out request {:scopes [{:name "Locals" :variablesReference 1}]})
              "variables" (dap-util/respond! out request {:variables [(variable "new" "value" 0)]})))
          (pause! view session events 1)
          (dap-util/await-ui! view #(item-at view ["new"]))
          (dap/close! old-session)
          (ui/run-now
            (is (nil? (item-at view ["old"])))
            (is (= "value" (:value (.getValue (item-at view ["new"])))))))))))

;; A load can finish before resume but render afterwards; a real continued event must prevent stale selection restoration.
(deftest resumed-session-does-not-restore-selection-test
  (with-view
    (fn [_ _ frame-id] [(variable "self" (str frame-id) 0)])
    (fn [_ _ _] [])
    (pause! view session events 1)
    (dap-util/await-ui! view #(item-at view ["self"]))
    (ui/run-now
      (.select (.getSelectionModel view) (item-at view ["self"]))
      (variables/clear! view session))
    (dap-util/await-ui! view #(nil? (.getRoot view)))
    (ui/run-now
      (dap-util/await! (variables/show-frame! view session (dap/suspension @session) 2))
      (dap-util/await!
        (future/io
          (dap/control! session "continue"))))
    (is (= [:continued] (dap-util/take-event! events)))
    (dap-util/await-ui! view #(= "2" (some-> (item-at view ["self"]) .getValue :value)))
    (ui/run-now
      (is (coll/empty? (ui/selection view))))))

;; Clearing removes stale values; same-frame refreshes preserve expansion and selection.
(deftest frame-refresh-test
  (let [loads (atom 0)]
    (with-view
      (fn [_ _ _]
        (swap! loads inc)
        [(variable "self" "table" 1)])
      (fn [_ _ _] [(variable "child" (str @loads) 0)])
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "child"]))
      (ui/run-now
        (.select (.getSelectionModel view) (item-at view ["self" "child"]))
        (variables/clear! view session))
      (dap-util/await-ui! view #(nil? (.getRoot view)))
      (doseq [load [2 3]]
        (ui/run-now (variables/show-frame! view session (dap/suspension @session) 1))
        (dap-util/await-ui! view #(= (str load) (some-> (item-at view ["self" "child"]) .getValue :value)))
        (ui/run-now
          (is (.isExpanded (item-at view ["self"])))
          (is (= "child" (:name (first (ui/selection view)))))))
      (is (= 3 @loads)))))

;; A coalesced same-frame refresh must keep expansion working without rendering the cleared tree.
(deftest unchanged-frame-refresh-test
  (let [root-requests (atom 0)]
    (with-view
      (fn [_ _ _]
        (swap! root-requests inc)
        [(variable "self" "table" 1)])
      (fn [_ _ _] [(variable "child" "value" 0)])
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now
        (let [root (.getRoot view)]
          (dap-util/await! (variables/show-frame! view session (dap/suspension @session) 1))
          (is (identical? root (.getRoot view)))
          (.setExpanded (item-at view ["self"]) true)))
      (dap-util/await-ui! view #(item-at view ["self" "child"]))
      (is (= 2 @root-requests))
      (ui/run-now (is (= "value" (:value (.getValue (item-at view ["self" "child"])))))))))

;; Expansion before rendering resolves the displayed item's intent against the refreshed frame.
(deftest current-reference-expansion-test
  (let [child-request (promise)]
    (with-view
      (fn [_ _ frame-id] [(variable "self" "table" frame-id)])
      (fn [request out reference]
        (deliver child-request [request out reference])
        nil)
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now
        (let [item (item-at view ["self"])]
          (dap-util/await! (variables/show-frame! view session (dap/suspension @session) 2))
          (is (= 1 (:variablesReference (.getValue item))))
          (.setExpanded item true)))
      (let [[request out reference] (dap-util/await! child-request)]
        (is (= 2 reference))
        (dap-util/respond! out request {:variables [(variable "child" "current" 0)]})
        (dap-util/await-ui! view #(item-at view ["self" "child"]))
        (ui/run-now (is (= "current" (:value (.getValue (item-at view ["self" "child"]))))))))))

;; Stale displayed tables must not save expansion after becoming scalar, absent, or cleared.
(deftest stale-item-expansion-test
  (doseq [shape [:scalar :absent :cleared]]
    (let [child-requests (atom [])]
      (with-view
        (fn [_ _ frame-id]
          (if (= 2 frame-id)
            (if (= :scalar shape) [(variable "self" "scalar" 0)] [])
            [(variable "self" "table" frame-id)]))
        (fn [_ _ reference] (swap! child-requests conj reference) [])
        (pause! view session events 1)
        (dap-util/await-ui! view #(item-at view ["self"]))
        (ui/run-now
          (let [item (item-at view ["self"])]
            (if (= :cleared shape)
              (variables/clear! view session)
              (dap-util/await! (variables/show-frame! view session (dap/suspension @session) 2)))
            (.setExpanded item true)))
        (dap-util/await! (ui/run-now (variables/show-frame! view session (dap/suspension @session) 3)))
        (dap-util/await-ui! view #(= 3 (some-> (item-at view ["self"]) .getValue :variablesReference)))
        (ui/run-now (is (not (.isExpanded (item-at view ["self"])))))
        (is (= [] @child-requests))))))

;; Recursive Alt expansion must not fetch reference zero or save expansion paths for scalar leaves.
(deftest recursive-expansion-skips-leaves-test
  (let [variable-requests (atom [])]
    (with-view
      (fn [_ _ frame-id] [(variable "self" "table" frame-id)])
      (fn [_ _ reference]
        (swap! variable-requests conj reference)
        (case (long reference)
          1 [(variable "scalar" "value" 0) (variable "nested" "table" 2)]
          2 [(variable "child" "value" 0)]
          3 [(variable "scalar" "table" 4) (variable "nested" "table" 2)]
          []))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "nested"]))
      (ui/run-now
        (.setExpanded (item-at view ["self"]) false)
        ;; Alt-click toggles every loaded descendant, including leaf TreeItems.
        (doseq [^TreeItem item (ui/tree-item-seq (item-at view ["self"]))]
          (.setExpanded item true)))
      (dap-util/await-ui! view #(item-at view ["self" "nested" "child"]))
      (is (= {1 1, 2 1} (frequencies @variable-requests)))
      (pause! view session events 3)
      (dap-util/await-ui! view #(item-at view ["self" "nested" "child"]))
      (ui/run-now (is (not (.isExpanded (item-at view ["self" "scalar"])))))
      (is (= {1 1, 2 2, 3 1} (frequencies @variable-requests))))))

;; Opened paths reload new references and values across stops; collapsed branches stay closed.
(deftest restore-expanded-paths-test
  (let [variable-requests (atom [])]
    (with-view
      (fn [_ _ generation]
        (let [base (* (long generation) 100)]
          [(variable "self" "table" (+ base 1))
           (variable "_G" "table" (+ base 3))
           (variable "unopened" "table" (+ base 99))]))
      (fn [_ _ reference]
        (let [generation (quot (long reference) 100)
              base (* generation 100)]
          (swap! variable-requests conj [generation reference])
          (cond
            (= reference (+ base 1)) [(variable "nested" "table" (+ base 2))
                                      (variable "cycle" "table" (+ base 1))]
            (= reference (+ base 2)) [(variable "count" (str generation) 0)]
            (= reference (+ base 3)) [(variable "global" (str generation) 0)])))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (is (= [] @variable-requests))
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
      (reset! variable-requests [])
      (pause! view session events 2)
      (dap-util/await-ui! view #(and (item-at view ["self" "nested" "count"])
                                     (item-at view ["self" "cycle" "cycle"])
                                     (item-at view ["_G" "global"])))
      (ui/run-now
        (is (= "2" (:value (.getValue (item-at view ["self" "nested" "count"])))))
        (is (.isExpanded (item-at view ["self" "cycle"])))
        (is (not (.isExpanded (item-at view ["self" "cycle" "cycle"]))))
        (is (not (.isExpanded (item-at view ["unopened"])))))
      (is (= {[2 201] 2 [2 202] 1 [2 203] 1} (frequencies @variable-requests)))
      (ui/run-now (.setExpanded (item-at view ["self" "nested"]) false))
      (reset! variable-requests [])
      (pause! view session events 3)
      (dap-util/await-ui! view #(and (item-at view ["self" "cycle" "cycle"])
                                     (item-at view ["_G" "global"])))
      (ui/run-now (is (not (.isExpanded (item-at view ["self" "nested"])))))
      (is (= {[3 301] 2 [3 303] 1} (frequencies @variable-requests))))))

;; Saved expansion survives a table becoming scalar or absent without fetching its children.
(deftest changing-variable-shapes-test
  (let [variable-requests (atom [])]
    (with-view
      (fn [_ _ generation]
        (case (long generation)
          2 [(variable "self" "nil" 0)]
          3 []
          [(variable "self" "table" generation)]))
      (fn [_ _ reference]
        (swap! variable-requests conj reference)
        [(variable "value" (str reference) 0)])
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "value"]))
      (pause! view session events 2)
      (dap-util/await-ui! view #(= "nil" (some-> (item-at view ["self"]) .getValue :value)))
      (dap-util/await! (pause! view session events 3))
      (dap-util/await-ui! view #(zero? (count (.getChildren (.getRoot view)))))
      (is (= [1] @variable-requests))
      (pause! view session events 4)
      (dap-util/await-ui! view #(item-at view ["self" "value"]))
      (ui/run-now (is (= "4" (:value (.getValue (item-at view ["self" "value"]))))))
      (is (= [1 4] @variable-requests)))))

;; A completed old frame refresh cannot overwrite another frame in the same suspension.
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
          (ui/run-now (variables/show-frame! view session snapshot 2))
          (dap-util/await-ui! view #(item-at view ["new"]))
          (finally
            (dap-util/respond! out request {:variables [(variable "old" "1" 0)]})
            (dap-util/await! old-refresh)))
        (ui/run-now
          (is (= ["new"] (mapv #(-> ^TreeItem % .getValue :name)
                               (.getChildren (.getRoot view)))))
          (is (= "2" (:value (.getValue (item-at view ["new"]))))))))))

;; Render the scene offscreen so JavaFX supplies cell geometry without showing a window.
(defn- render-view! [^TreeView view]
  (.snapshot (.getScene view) nil))

(defn- viewport [^TreeView view]
  (render-view! view)
  (let [skin ^ExtendedTreeViewSkin (.getSkin view)
        flow (.getVirtualFlowInstance skin)
        cell ^TreeCell (.getFirstVisibleCell flow)]
    {:name (some-> cell .getTreeItem .getValue :name)
     :row (some-> cell .getIndex)
     :selection (:name (first (ui/selection view)))}))

(defn- item-visible? [^TreeView view ^TreeItem item]
  (render-view! view)
  (let [skin ^ExtendedTreeViewSkin (.getSkin view)
        flow (.getVirtualFlowInstance skin)
        row (.getRow view item)]
    (<= (.getIndex ^TreeCell (.getFirstVisibleCell flow))
        row
        (.getIndex ^TreeCell (.getLastVisibleCell flow)))))

;; Reusing the variables view for a new session must reset expansion, selection, and scroll position.
(deftest new-session-resets-tree-state-test
  (let [variable-requests (atom [])]
    (with-view
      (fn [_ _ _] [(variable "self" "table" 1)])
      (fn [_ _ _] (mapv #(variable (str "field-" %) "value" 0) (range 80)))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "field-79"]))
      (ui/run-now
        (render-view! view)
        (.select (.getSelectionModel view) (item-at view ["self" "field-44"]))
        (.scrollTo view 40)
        (is (= "field-44" (:selection (viewport view))))
        (is (not= "self" (:name (viewport view)))))
      (dap/close! session)
      (ui/run-now (variables/clear! view session))
      (dap-util/with-adapter
        "/project"
        {}
        (fn [request _ out _socket]
          (case (:command request)
            "evaluate"
            (do
              (dap-util/event! out "stopped" {:threadId 7})
              (dap-util/respond! out request {}))

            "scopes"
            (dap-util/respond! out request {:scopes [{:name "Locals" :variablesReference 10001}]})

            "variables"
            (let [reference (get-in request [:arguments :variablesReference])]
              (swap! variable-requests conj reference)
              (dap-util/respond! out request
                                 {:variables (into [(variable "self" "table" 1)]
                                                   (map #(variable (str "new-" %) "value" 0))
                                                   (range 80))}))))
        (pause! view session events 1)
        (dap-util/await-ui! view #(item-at view ["self"]))
        (ui/run-now
          (render-view! view)
          (is (not (.isExpanded (item-at view ["self"]))))
          (is (coll/empty? (ui/selection view)))
          (is (= 0 (:row (viewport view))))
          (is (= "self" (:name (viewport view)))))
        (is (= [10001] @variable-requests))))))

;; A coalesced session change must discard native selection even when the new rows have equal values.
(deftest coalesced-session-change-test
  (with-view
    (fn [_ _ _] [(variable "value" "same" 0)])
    (fn [_ _ _] [])
    (pause! view session events 1)
    (dap-util/await-ui! view #(item-at view ["value"]))
    (let [root (ui/run-now
                 (.select (.getSelectionModel view) (item-at view ["value"]))
                 (.getRoot view))]
      (dap-util/with-adapter
        "/project"
        {}
        (fn [request _ out _socket]
          (case (:command request)
            "evaluate" (do (dap-util/event! out "stopped" {:threadId 7})
                           (dap-util/respond! out request {}))
            "scopes" (dap-util/respond! out request {:scopes [{:name "Locals" :variablesReference 10001}]})
            "variables" (dap-util/respond! out request {:variables [(variable "value" "same" 0)]})))
        (dap/evaluate! session nil "pause")
        (let [[_ snapshot] (dap-util/take-event! events)]
          (ui/run-now
            (dap-util/await! (variables/show-frame! view session snapshot 1))
            (is (identical? root (.getRoot view))))
          (dap-util/await-ui! view #(and (item-at view ["value"])
                                         (not (identical? root (.getRoot view)))))
          (ui/run-now
            (is (coll/empty? (ui/selection view)))
            (is (= "same" (:value (.getValue (item-at view ["value"])))))))))))

;; Declarative setup preserves the skin across refreshes and forwards Space to editor shortcuts.
(deftest skin-and-space-shortcut-test
  (let [space-presses (atom 0)]
    (with-view
      (fn [_ _ generation] [(variable "value" (str generation) 0)])
      (fn [_ _ _] [])
      (let [skin (ui/run-now (.getSkin view))]
        (ui/run-now
          (.setOnKeyPressed (.getRoot (.getScene view))
                            (ui/event-handler event
                              (when (= KeyCode/SPACE (.getCode ^KeyEvent event))
                                (swap! space-presses inc)))))
        (doseq [generation [1 2]]
          (pause! view session events generation)
          (dap-util/await-ui! view #(= (str generation) (some-> (item-at view ["value"]) .getValue :value)))
          (ui/run-now
            (is (identical? skin (.getSkin view)))
            (.requestFocus view)
            (plorer/key-tap! (.getScene view) :home)
            (plorer/key-tap! (.getScene view) :space)
            (is (= "value" (:name (first (ui/selection view))))))
          (is (= generation @space-presses)))))))

;; Focus-only keyboard navigation survives rendering and load completion without reselecting the saved item.
(deftest keyboard-focus-survives-loading-test
  (let [pending (promise)]
    (with-view
      (fn [_ _ _] [(variable "first" "scalar" 0)
                   (variable "table" "table" 1)
                   (variable "last" "scalar" 0)])
      (fn [request out _] (deliver pending [request out]) nil)
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["table"]))
      (ui/run-now (.setExpanded (item-at view ["table"]) true))
      (let [[request out] (dap-util/await! pending)
            modifier (if (.startsWith (System/getProperty "os.name") "Mac") :meta :control)]
        (ui/run-now
          (render-view! view)
          (.requestFocus view)
          (plorer/key-tap! (.getScene view) :home)
          (plorer/key-chord! (.getScene view) [modifier :down])
          (is (= "first" (:name (first (ui/selection view)))))
          (is (= "table" (:name (.getValue ^TreeItem (.getFocusedItem (.getFocusModel view)))))))
        (ui/run-now
          (is (= "table" (:name (.getValue ^TreeItem (.getFocusedItem (.getFocusModel view)))))))
        (dap-util/respond! out request {:variables [(variable "child" "value" 0)]})
        (dap-util/await-ui! view #(item-at view ["table" "child"]))
        (ui/run-now
          (is (= "first" (:name (first (ui/selection view)))))
          (is (= "table" (:name (.getValue ^TreeItem (.getFocusedItem (.getFocusModel view))))))
          (plorer/key-chord! (.getScene view) [modifier :down]))
        (ui/run-now
          (is (= "child" (:name (.getValue ^TreeItem (.getFocusedItem (.getFocusModel view)))))))))))

;; A real rejected request is reported through the console stream and retries only on explicit expansion.
(deftest failed-child-load-can-retry-test
  (test-support/with-clean-system
    (console/clear-console!)
    (let [pending (promise)
          variable-requests (atom [])
          lines (LinkedBlockingQueue.)
          console-node (g/make-node! console/ConsoleNode)
          console-view (g/make-node! console/ConsoleView :gutter-view (console/->ConsoleGutterView))
          _ (g/transact (g/connect console-node :_node-id console-view :resource-node))
          response ((get-in (console/routes console-view) ["/console/stream" "GET"]) {})
          out (proxy [ByteArrayOutputStream] []
                (flush []
                  (run! #(.add lines %) (string/split-lines (.toString ^ByteArrayOutputStream this StandardCharsets/UTF_8)))
                  (.reset ^ByteArrayOutputStream this)))
          stream (.start (Thread/ofVirtual)
                         ^Runnable
                         (fn []
                           (try
                             (http-server.types/connection-write! (:body response) out)
                             (catch InterruptedException _))))]
      (try
        (console/append-console-line! "variables-stream-ready")
        (is (= "variables-stream-ready" (dap-util/take-event! lines)))
        (with-view
          (fn [_ _ _] [(variable "failed" "table" 1) (variable "other" "table" 2)])
          (fn [request out reference]
            (swap! variable-requests conj reference)
            (case (long reference)
              1 (if-not (= 1 (get (frequencies @variable-requests) 1))
                  [(variable "retried" "value" 0)]
                  (do (dap-util/reject! out request "temporary failure") nil))
              2 (do (deliver pending [request out]) nil)))
          (pause! view session events 1)
          (dap-util/await-ui! view #(item-at view ["failed"]))
          (ui/run-now
            (.setExpanded (item-at view ["failed"]) true)
            (.setExpanded (item-at view ["other"]) true))
          (is (.contains ^String (dap-util/take-event! lines) "temporary failure"))
          (let [[request out] (dap-util/await! pending)]
            (dap-util/respond! out request {:variables [(variable "loaded" "value" 0)]})
            (dap-util/await-ui! view #(item-at view ["other" "loaded"]))
            (is (= {1 1, 2 1} (frequencies @variable-requests)))
            (ui/run-now
              (is (not (.isLeaf (item-at view ["failed"]))))
              (.setExpanded (item-at view ["failed"]) false)
              (.setExpanded (item-at view ["failed"]) true))
            (dap-util/await-ui! view #(item-at view ["failed" "retried"]))
            (is (= {1 2, 2 1} (frequencies @variable-requests)))))
        (finally
          (.interrupt stream)
          (.join stream)
          (console/clear-console!))))))

;; Collapsing a table while children load must keep it closed and avoid restoring it at the next stop.
(deftest collapse-during-load-test
  (let [pending (promise)
        child-requests (atom [])]
    (with-view
      (fn [_ _ frame-id] [(variable "self" "table" frame-id)])
      (fn [request out reference]
        (swap! child-requests conj reference)
        (if (= 1 reference)
          (do (deliver pending [request out]) nil)
          [(variable "child" "value" 0)]))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (let [[request out] (dap-util/await! pending)]
        (ui/run-now (.setExpanded (item-at view ["self"]) false))
        (dap-util/respond! out request {:variables [(variable "child" "value" 0)]}))
      (dap-util/await-ui! view #(item-at view ["self" "child"]))
      (ui/run-now (is (not (.isExpanded (item-at view ["self"])))))
      (dap-util/await! (pause! view session events 2))
      (dap-util/await-ui! view #(= 2 (some-> (item-at view ["self"]) .getValue :variablesReference)))
      (ui/run-now (is (not (.isExpanded (item-at view ["self"])))))
      (is (= [1] @child-requests))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "child"]))
      (is (= [1 2] @child-requests)))))

;; Clearing selection with the keyboard survives a pending wire response and the next stop.
(deftest cleared-selection-survives-loading-test
  (let [pending (promise)]
    (with-view
      (fn [_ _ frame-id] [(variable "self" "table" frame-id) (variable "other" "scalar" 0)])
      (fn [request out reference]
        (if (realized? pending)
          [(variable "child" (str reference) 0)]
          (do (deliver pending [request out]) nil)))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (let [[request out] (dap-util/await! pending)]
        (ui/run-now
          (render-view! view)
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
        (pause! view session events 2)
        (dap-util/await-ui! view #(= "2" (some-> (item-at view ["self" "child"]) .getValue :value)))
        (ui/run-now (is (coll/empty? (ui/selection view))))))))

;; Repeated clears of an empty tree must preserve the pending scroll position for the next refresh.
(deftest repeated-clear-preserves-scroll-position-test
  (with-view
    (fn [_ _ frame-id] (mapv #(variable (str "field-" %) (str frame-id) 0) (range 80)))
    (fn [_ _ _] [])
    (pause! view session events 1)
    (dap-util/await-ui! view #(item-at view ["field-79"]))
    (let [row (ui/run-now
                (render-view! view)
                (.scrollTo view 40)
                (:row (viewport view)))]
      (is (= 40 row))
      (ui/run-now (variables/clear! view session))
      (dap-util/await-ui! view #(nil? (.getRoot view)))
      (ui/run-now
        (render-view! view)
        (variables/clear! view session))
      (pause! view session events 2)
      (dap-util/await-ui! view #(= "2" (some-> (item-at view ["field-79"]) .getValue :value)))
      (ui/run-now
        (is (= row (:row (viewport view))))))))

;; Navigation before queued advancement must cancel restoration even when the callback has old state.
(deftest navigation-before-refresh-render-cancels-scroll-restoration-test
  (let [pending (promise)
        values (fn [frame-id] (mapv #(variable (str "field-" %) (str frame-id) 0) (range 80)))]
    (with-view
      (fn [request out frame-id]
        (if (= 2 frame-id)
          (do (deliver pending [request out]) nil)
          (values frame-id)))
      (fn [_ _ _] [])
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["field-79"]))
      (let [row (ui/run-now
                  (render-view! view)
                  (.scrollTo view 40)
                  (:row (viewport view)))]
        (is (= 40 row))
        (ui/run-now
          (variables/show-frame! view session (dap/suspension @session) 2)
          (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/END false false false false)))
        (let [[request out] (dap-util/await! pending)]
          (dap-util/await-ui! view #(zero? (count (.getChildren (.getRoot view)))))
          (ui/run-now (render-view! view))
          (dap-util/respond! out request {:variables (values 2)})
          (dap-util/await-ui! view #(= "2" (some-> (item-at view ["field-79"]) .getValue :value)))
          (ui/run-now
            (is (not= row (:row (viewport view))))))))))

;; Refresh restores the saved row index even when every variable has a new name.
(deftest renamed-variables-restore-scroll-row-test
  (with-view
    (fn [_ _ frame-id] (mapv #(variable (str frame-id "-field-" %) "value" 0) (range 80)))
    (fn [_ _ _] [])
    (pause! view session events 1)
    (dap-util/await-ui! view #(item-at view ["1-field-79"]))
    (ui/run-now
      (viewport view)
      (.scrollTo view 40)
      (is (= 40 (:row (viewport view))))
      (variables/clear! view session))
    (dap-util/await-ui! view #(nil? (.getRoot view)))
    (ui/run-now (viewport view))
    (pause! view session events 2)
    (dap-util/await-ui! view #(item-at view ["2-field-79"]))
    (ui/run-now
      (is (nil? (item-at view ["1-field-79"])))
      (let [after (viewport view)]
        (is (= 40 (:row after)))
        (is (= "2-field-40" (:name after)))))))

;; Restoring a scrolled tree after it shrinks must leave its remaining rows visible.
(deftest shrinking-tree-restores-scroll-position-test
  (with-view
    (fn [_ _ frame-id]
      (if (= 1 frame-id)
        (mapv #(variable (str "field-" %) "value" 0) (range 80))
        [(variable "remaining" "value" 0)]))
    (fn [_ _ _] [])
    (pause! view session events 1)
    (dap-util/await-ui! view #(item-at view ["field-79"]))
    (ui/run-now
      (render-view! view)
      (.scrollTo view 40))
    (ui/run-now (is (item-visible? view (item-at view ["field-40"]))))
    (pause! view session events 2)
    (dap-util/await-ui! view #(item-at view ["remaining"]))
    (ui/run-now
      (is (nil? (item-at view ["field-40"])))
      (is (item-visible? view (item-at view ["remaining"]))))))

;; Refresh retains the first visible row after insertion; navigation overrides delayed restoration.
(deftest restore-scroll-position-test
  (let [pending (promise)
        values (fn [generation]
                 (into (if (= 1 generation) [] [(variable "new sibling" "added" 0)])
                       (map #(variable (str "field-" %) (str generation) 0))
                       (range 100)))]
    (with-view
      (fn [_ _ generation] [(variable "self" "table" generation) (variable "other" "scalar" 0)])
      (fn [request out reference]
        (if (= 3 reference)
          (do (deliver pending [request out]) nil)
          (values reference)))
      (pause! view session events 1)
      (dap-util/await-ui! view #(item-at view ["self"]))
      (ui/run-now (.setExpanded (item-at view ["self"]) true))
      (dap-util/await-ui! view #(item-at view ["self" "field-99"]))
      (let [before
            (ui/run-now
              (render-view! view)
              (.select (.getSelectionModel view) (item-at view ["self" "field-44"]))
              (.scrollTo view 40)
              (viewport view))]
        (is (= "field-44" (:selection before)))
        (pause! view session events 2)
        (dap-util/await-ui! view #(and (= "2" (some-> (item-at view ["self" "field-99"]) .getValue :value))
                                       (item-at view ["self" "new sibling"])))
        (ui/run-now
          (let [after (viewport view)]
            (is (= (:row before) (:row after)))
            (is (= (:selection before) (:selection after)))))
        (pause! view session events 3)
        (let [[request out] (dap-util/await! pending)]
          (dap-util/await-ui! view #(= 3 (some-> (item-at view ["self"]) .getValue :variablesReference)))
          (ui/run-now
            (render-view! view)
            (.select (.getSelectionModel view) (item-at view ["self"]))
            (.fireEvent view (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/END false false false false))
            (is (= "other" (:name (first (ui/selection view))))))
          (dap-util/respond! out request {:variables (values 3)})
          (dap-util/await-ui! view #(= "3" (some-> (item-at view ["self" "field-99"]) .getValue :value)))
          (ui/run-now
            (render-view! view)
            (is (= "other" (:name (first (ui/selection view)))))
            (is (not= (:name before) (:name (viewport view))))))))))
