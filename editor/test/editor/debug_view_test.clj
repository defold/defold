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

(ns editor.debug-view-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.console :as console]
            [editor.debug-view :as debug-view]
            [editor.debugging.dap :as dap]
            [editor.debugging.variables :as debugger-variables]
            [editor.defold-project :as project]
            [editor.engine :as engine]
            [editor.os :as os]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [support.dap-util :as dap-util]
            [support.test-support :as test-support]
            [util.http-server.types :as http-server.types])
  (:import [java.io ByteArrayOutputStream]
           [java.nio.charset StandardCharsets]
           [java.util Collection]
           [java.util.concurrent LinkedBlockingQueue]
           [javafx.scene Scene]
           [javafx.scene.control ListView TextField TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout StackPane]
           [javafx.stage Stage]
           [org.luaj.vm2.lib.jse JsePlatform]))

(set! *warn-on-reflection* true)

(g/defnode ConsoleStreamView
  (property resource-node g/NodeID))

(g/defnode BreakpointSource
  (property breakpoints project/Breakpoints))

(defn- await-state! [changes predicate]
  (loop []
    (when-not (ui/run-now (predicate))
      (.take ^LinkedBlockingQueue changes)
      (recur))))

;; Start and replacement convert enabled editor breakpoints to DAP lines and
;; conditions; invalidation refreshes the selected frame, and detach closes the session.
(deftest debugger-session-lifecycle-test
  (test-support/with-clean-system
    (let [workspace (workspace/make-workspace "test/resources/empty_project" {} {} nil)
          project (g/make-node! project/Project :workspace workspace)
          script (workspace/file-resource workspace "/main.script")
          breakpoints (g/make-node! BreakpointSource
                        :breakpoints [{:resource script :row 4 :condition "x > 2" :enabled true}
                                      {:resource script :row 1 :enabled false}
                                      {:resource script :row 2 :condition " " :enabled true}])
          _ (g/transact (g/connect breakpoints :breakpoints project :breakpoints))
          changes (LinkedBlockingQueue.)
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (debugger-variables/make-view!))
          view (g/make-node! debug-view/DebugView
                 :call-stack-view call-stack
                 :variables-view variables
                 :state-changed-fn (fn [_] (.add changes true)))]
      (dap-util/with-server
        (fn [{:keys [command arguments] :as request} _ out _]
          (case command
            "threads" (dap-util/respond! out request {:threads [{:id 7 :name "Lua"}]})
            "pause" (do (dap-util/respond! out request {}) (dap-util/event! out "stopped" {:threadId 7}))
            "stackTrace" (dap-util/respond! out request {:stackFrames [{:id 42 :name "update" :line 5}
                                                                       {:id 99 :name "caller" :line 9}]})
            "scopes" (dap-util/respond! out request {:scopes [{:name "Locals" :variablesReference (:frameId arguments)}]})
            "variables" (dap-util/respond! out request {:variables [{:name "frame" :value (str (:variablesReference arguments)) :variablesReference 0}]})
            "evaluate" (do (dap-util/event! out "invalidated" {:areas ["variables"]}) (dap-util/respond! out request {}))
            "continue" (do (dap-util/respond! out request {}) (dap-util/event! out "continued" {:threadId 7}))))
        (debug-view/start-debugger! view project {:address "127.0.0.1" :debugger-port port} false)
        (let [first-session (debug-view/current-session view)]
          (try
            (await-state! changes #(= :running (dap/status first-session)))
            (is (= [{:line 3} {:line 5 :condition "x > 2"}]
                   (get-in (first (filterv #(= "setBreakpoints" (:command %)) @requests)) [:arguments :breakpoints])))
            (is (false? (get-in (first (filterv #(= "attach" (:command %)) @requests)) [:arguments :stopOnEntry])))
            (dap/control! first-session "pause")
            (await-state! changes #(g/with-auto-evaluation-context ec (debug-view/suspended? view ec)))
            (ui/run-now
              (g/node-value view :update-call-stack)
              (is (= [42] (mapv :id (ui/selection call-stack)))))
            (doseq [frame-id [42 99]]
              (ui/run-now (ui/select! call-stack (first (filterv #(= frame-id (:id %)) (ui/items call-stack)))))
              (dap/evaluate! first-session frame-id "refresh")
              (dap-util/await-ui!
                variables
                #(when-let [^TreeItem item (some-> (.getRoot variables) .getChildren first)]
                   (= (str frame-id) (:value (.getValue item))))))
            (dap/control! first-session "continue")
            (await-state! changes #(not (g/with-auto-evaluation-context ec (debug-view/suspended? view ec))))
            (g/set-property! breakpoints :breakpoints [{:resource script :row 9 :enabled true}])
            (dap-util/with-server
              (fn [request _ out _] (dap-util/respond! out request {}))
              (dap-util/await! (debug-view/start-debugger! view project {:address "127.0.0.1" :debugger-port port} true))
              (let [next-session (debug-view/current-session view)]
                (try
                  (await-state! changes #(= :running (dap/status next-session)))
                  (is (= [{:line 10}]
                         (get-in (first (filterv #(= "setBreakpoints" (:command %)) @requests)) [:arguments :breakpoints])))
                  (is (not (identical? first-session next-session)))
                  (is (= :closed (dap/status first-session)))
                  (is (identical? next-session (debug-view/current-session view)))
                  (dap-util/await! (debug-view/detach! view))
                  (await-state! changes #(nil? (debug-view/current-session view)))
                  (is (= :closed (dap/status next-session)))
                  (finally (dap/close! next-session)))))
            (finally
              (dap/close! first-session)
              (ui/run-now (debugger-variables/clear! variables)))))))))

;; A connecting debugger re-reads launched-target metadata when the listener
;; port arrives later, rather than retaining the initial portless target.
(deftest late-debugger-port-test
  (test-support/with-clean-system
    (let [workspace (workspace/make-workspace "test/resources/empty_project" {} {} nil)
          project (g/make-node! project/Project :workspace workspace)
          changes (LinkedBlockingQueue.)
          view (g/make-node! debug-view/DebugView :state-changed-fn (fn [_] (.add changes true)))
          ^Stage stage (ui/run-now (doto (Stage.) (.setScene (Scene. (StackPane.)))))
          command (if (os/is-win32?)
                    ["cmd" "/c" "set /p value="]
                    ["sh" "-c" "read value"])
          process (.start (ProcessBuilder. ^java.util.List command))]
      (try
        (binding [ui/*main-stage* (atom stage)]
          (let [target (targets/add-launched-target! 1 {:process process :address "127.0.0.1"})
                removed (promise)
                cancel-watch (targets/when-url-or-removed (:id target) #(deliver removed %))]
            (try
              (dap-util/with-server
                (fn [request _ out _] (dap-util/respond! out request {}))
                (debug-view/start-debugger! view project target false)
                (let [session (debug-view/current-session view)]
                  (try
                    (is (= :connecting (dap/status session)))
                    (targets/update-launched-target! target
                                                     (engine/parse-launched-target-info
                                                       (str "Lua DAP debugger port: " port)))
                    (await-state! changes #(= :running (dap/status session)))
                    (is (identical? session (debug-view/current-session view)))
                    (finally (dap/close! session)))))
              (finally
                (.destroyForcibly process)
                (.waitFor process)
                (try
                  (is (nil? (dap-util/await! removed)))
                  (finally (cancel-watch)))))))
        (finally
          (.destroyForcibly process)
          (ui/run-now (.close stage)))))))

;; A stack reply arriving after continue cannot restore the previous stop in the editor.
(deftest stale-stack-response-test
  (test-support/with-clean-system
    (let [workspace (workspace/make-workspace "test/resources/empty_project" {} {} nil)
          project (g/make-node! project/Project :workspace workspace)
          changes (LinkedBlockingQueue.)
          pending (promise)
          view (g/make-node! debug-view/DebugView :state-changed-fn (fn [_] (.add changes true)))]
      (dap-util/with-server
        (fn [{:keys [command] :as request} _ out _]
          (case command
            "threads" (dap-util/respond! out request {:threads [{:id 7 :name "Lua"}]})
            "pause" (do (dap-util/respond! out request {}) (dap-util/event! out "stopped" {:threadId 7}))
            "stackTrace" (if (realized? pending)
                           (dap-util/respond! out request {:stackFrames [{:id 99 :name "fresh" :line 9}]})
                           (deliver pending [request out]))
            "continue" (do (dap-util/event! out "continued" {:threadId 7}) (dap-util/respond! out request {}))))
        (debug-view/start-debugger! view project {:address "127.0.0.1" :debugger-port port} false)
        (let [session (debug-view/current-session view)]
          (try
            (await-state! changes #(= :running (dap/status session)))
            (dap/control! session "pause")
            (let [[request out] (dap-util/await! pending)]
              (dap/control! session "continue")
              (dap-util/respond! out request {:stackFrames [{:id 42 :name "stale" :line 5}]})
              (dap/control! session "pause")
              (await-state! changes #(g/with-auto-evaluation-context ec (debug-view/suspended? view ec)))
              (is (= [99] (mapv :id (:stack (g/node-value view :suspension-state))))))
            (finally (dap/close! session))))))))

;; Prompt evaluation uses the selected frame during selection notifications,
;; and the public console stream contains the evaluated table's formatted result.
(deftest table-evaluation-prompt-test
  (test-support/with-clean-system
    (console/clear-console!)
    (let [evaluating (LinkedBlockingQueue.)
          lines (LinkedBlockingQueue.)
          console-node (g/make-node! console/ConsoleNode)
          console-view (g/make-node! ConsoleStreamView :resource-node console-node)
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
        (console/append-console-line! "stream-ready")
        (is (= "stream-ready" (dap-util/take-event! lines)))
        (dap-util/with-adapter
          {}
          (fn [{:keys [command arguments] :as request} _ out _]
            (case command
              "evaluate" (if (= "stop" (:expression arguments))
                           (do (dap-util/event! out "stopped" {:threadId 7}) (dap-util/respond! out request {}))
                           (.add evaluating [request out]))
              "variables" (do
                            (is (= 1 (:variablesReference arguments)))
                            (dap-util/respond! out request {:variables [{:name "answer" :value "42" :variablesReference 0}]}))))
          (dap/evaluate! session nil "stop")
          (dap-util/take-event! events)
          (let [^ListView call-stack (ui/run-now (ListView.))
                ^TextField prompt (ui/run-now (TextField.))
                view (g/make-node! debug-view/DebugView :debug-session session :call-stack-view call-stack)]
            (ui/run-now
              (debug-view/setup-prompt-field! view prompt)
              (ui/observe-selection call-stack
                                    (fn [_ frames]
                                      (when (pos? (count frames))
                                        (.setText prompt "{answer = 42}")
                                        (.fireEvent prompt (KeyEvent. KeyEvent/KEY_PRESSED "" "" KeyCode/ENTER false false false false))))))
            (doseq [[frames frame-id] [[[{:id 42} {:id 43}] 42]
                                       [[{:id 99} {:id 100}] 99]
                                       [[{:id 99} {:id 100}] 100]]]
              (ui/run-now
                (.setAll (.getItems call-stack) ^Collection frames)
                (ui/select! call-stack {:id frame-id}))
              (let [[request out] (dap-util/take-event! evaluating)]
                (is (= {:expression "{answer = 42}" :context "repl" :frameId frame-id} (:arguments request)))
                (ui/run-now
                  (is (= "" (.getText prompt)))
                  (is (= ["{answer = 42}"] (g/node-value view :evaluation-history))))
                (dap-util/respond! out request {:result "table: result" :variablesReference 1})
                (is (= ["{answer = 42}" "{ -- table: result" "  answer = 42" "}"]
                       (mapv (fn [_] (dap-util/take-event! lines)) (range 4))))))))
        (finally
          (.interrupt stream)
          (.join stream)
          (console/clear-console!))))))

;; UI-thread cancellation is rejected before mutating an active real session.
(deftest debugger-thread-affinity-test
  (dap-util/with-adapter
    {}
    (fn [request _ out _] (dap-util/respond! out request {}))
    (is (thrown? AssertionError (ui/run-now (dap/close! session))))
    (is (thrown? AssertionError (ui/run-now (dap/disconnect! session))))
    (is (= :running (dap/status session)))))

;; Execute the shipped Lua scripts to verify per-instance ports and remote
;; binding, and ensure discovery reports the returned port of an existing listener.
(deftest attach-startup-script-test
  (doseq [[filename expected-address] [["start.lua" "nil"]
                                       ["start_remote.lua" "0.0.0.0"]]]
    (let [globals (JsePlatform/standardGlobals)
          source (slurp (io/file "bundle-resources/_defold/debugger" filename))]
      (.call (.load globals (str "sys = {get_config_int = function() return 3 end}\n"
                                 "debugger = {start = function(port, address)\n"
                                 "  requested_port, requested_address = port, address\n"
                                 "  return 49152\n"
                                 "end}\n"
                                 "print = function(value) output = value end\n"
                                 source)))
      (is (= 8175 (.toint (.get globals "requested_port"))))
      (is (= expected-address (.tojstring (.get globals "requested_address"))))
      (is (= 49152 (:debugger-port (engine/parse-launched-target-info (.tojstring (.get globals "output")))))))))
