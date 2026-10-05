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
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.console :as console]
            [editor.debug-view :as debug-view]
            [editor.debugging.dap :as dap]
            [editor.debugging.variables :as debugger-variables]
            [editor.defold-project :as project]
            [editor.engine :as engine]
            [editor.future :as future]
            [editor.resource :as resource]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [support.test-support :as test-support])
  (:import [java.io IOException]
           [java.util Collection]
           [javafx.scene.control ListView TreeItem TreeView]
           [org.luaj.vm2.lib.jse JsePlatform]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- await! [value]
  (let [result (deref value 10000 ::timeout)]
    (when (= ::timeout result)
      (throw (IllegalStateException. "Timed out waiting for debugger UI test")))
    result))

;; Wrong-thread calls fail before cancellation or UI work, guarding against
;; blocking the UI during session closure and rendering errors from a worker.
(deftest debugger-thread-affinity-test
  (let [stop-requested (future/make)
        data {:debugger {:status :connecting}
              :transport {:work ::queue :stop-requested stop-requested}}
        session (dap/->Session (atom data)
                               (future/failed (IOException. "Already ended")))]
    (ui/run-now
      (is (thrown? AssertionError (dap/close! session)))
      (is (thrown? AssertionError (dap/disconnect! session)))
      (is (identical? data @(:data session)))
      (is (not (future/done? stop-requested))))
    (is (thrown? AssertionError
                 (debug-view/show-connect-failed-info! (IOException. "Wrong thread") ::workspace)))))

;; Verify callbacks from a closed session cannot clear a newer session, while
;; closing the current session clears its state within the dispatched UI callback.
(deftest stale-session-callback-test
  (test-support/with-clean-system
    (let [old (agent {:status :closed})
          current
          (agent {:status :suspended
                  :generation 1})
          view
          (g/make-node! debug-view/DebugView
            :debug-session current
            :suspension-state {:stack []}
            :state-changed-fn (constantly nil))
          callbacks (#'debug-view/make-debugger-callbacks view)]
      (ui/run-now
        ((:on-closed callbacks) old @old)
        ((:on-resumed callbacks) old @old)
        (is (identical? current (g/node-value view :debug-session)))
        (is (= {:stack []} (g/node-value view :suspension-state)))
        ((:on-closed callbacks) current @current)
        (is (nil? (g/node-value view :debug-session)))
        (is (nil? (g/node-value view :suspension-state)))))))

;; Errors delivered after a session is replaced must not report a failure for the
;; new connection; the current session's error is shown within its UI callback.
(deftest stale-session-error-test
  (test-support/with-clean-system
    (let [old (atom {:status :closed})
          current (atom {:status :running})
          callbacks (atom nil)
          errors (atom [])
          view (g/make-node! debug-view/DebugView
                 :state-changed-fn (constantly nil))]
      (with-redefs-fn
        {#'debug-view/collect-enabled-breakpoints (constantly #{})
         #'project/workspace (constantly ::workspace)
         #'workspace/project-directory (constantly (io/file "."))
         #'dap/connect!
         (fn [_ _ handlers]
           (reset! callbacks handlers)
           old)
         #'debug-view/show-connect-failed-info!
         (fn [exception workspace]
           (is (ui/on-ui-thread?))
           (is (= ::workspace workspace))
           (swap! errors conj (ex-message exception)))}
        #(ui/run-now
           (#'debug-view/connect-debugger! view ::project {:id "target"} false)
           (let [on-error (:on-error @callbacks)]
             (g/set-property! view :debug-session current)
             (on-error old @old (IOException. "Old connection failed"))
             (is (= [] @errors))
             (g/set-property! view :debug-session old)
             (on-error old @old (IOException. "Current connection failed"))
             (is (= ["Current connection failed"] @errors))))))))

;; A delayed suspension notification keeps the snapshot captured for its event,
;; guarding against querying frames from a later suspension of the same session.
(deftest suspension-callback-snapshot-test
  (let [session (atom {:status :suspended :generation 2 :threadId 7})
        captured (atom nil)
        callbacks (#'debug-view/make-debugger-callbacks ::view)]
    (with-redefs-fn
      {#'debug-view/update-suspension-state!
       (fn [view current snapshot]
         (is (= ::view view))
         (is (identical? session current))
         (reset! captured snapshot))}
      #((:on-suspended callbacks) session
                                  {:status :suspended :generation 1 :threadId 9} {:threadId 9}))
    (is (= {:generation 1 :threadId 9} @captured))))

;; Verify a stack response received after execution resumes cannot restore the
;; old suspension state in the editor.
(deftest stale-stack-response-test
  (test-support/with-clean-system
    (let [session
          (agent {:status :suspended
                  :generation 1
                  :threadId 7})
          view
          (g/make-node! debug-view/DebugView
            :debug-session session
            :state-changed-fn (constantly nil))
          started (promise)
          response (promise)]
      (with-redefs [dap/stack
                    (fn [_ _]
                      (deliver started true)
                      (await! response))]
        (let [work (#'debug-view/update-suspension-state! view session (dap/suspension @session))]
          (await! started)
          (await (send-via future/io-executor session assoc :status :running :generation 2))
          (deliver response [{:id 42
                              :file "/main.script"
                              :line 5}])
          (await! work)
          (ui/run-now
            (is (nil? (g/node-value view :suspension-state)))))))))

;; Verify switching frames while a variables request is pending keeps the new
;; frame's values, even when the old request finishes later.
(deftest selected-frame-variables-test
  (test-support/with-clean-system
    (let [session
          (agent {:status :suspended
                  :generation 1
                  :threadId 7})
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (debugger-variables/make-view!))
          view
          (g/make-node! debug-view/DebugView
            :debug-session session
            :call-stack-view call-stack
            :variables-view variables)
          started (promise)
          response (promise)]
      (ui/run-now
        (.add (.getItems call-stack) {:id 42})
        (.add (.getItems call-stack) {:id 99})
        (.select (.getSelectionModel call-stack) (int 0)))
      (with-redefs [dap/frame-variables
                    (fn [_ _ frame-id]
                      (if-not (= 42 frame-id)
                        [{:name "second"
                          :value "false"
                          :variablesReference 0}]
                        (do
                          (deliver started true)
                          (await! response))))]
        (let [first-work
              (ui/run-now (#'debug-view/load-frame-variables! view))]
          (await! started)
          (let [second-work
                (ui/run-now
                  (.select (.getSelectionModel call-stack) (int 1))
                  (#'debug-view/load-frame-variables! view))]
            (await! second-work)
            (deliver response [{:name "first"
                                :value "1"
                                :variablesReference 0}])
            (await! first-work)
            (ui/run-now
              (let [items (.getChildren (.getRoot variables))]
                (is (= 1 (count items)))
                (is (= "second" (:display-name (.getValue ^TreeItem (first items)))))
                (is (= "false" (:display-value (.getValue ^TreeItem (first items)))))))))))))

;; Verify selection listeners read the newly selected frame when JavaFX's
;; selectedItem is still stale, including a fast step that replaces the stack.
(deftest frame-selection-listener-test
  (test-support/with-clean-system
    (let [^ListView call-stack (ui/run-now (ListView.))
          view (g/make-node! debug-view/DebugView :call-stack-view call-stack)
          ^Collection initial-frames [{:id 42} {:id 43}]
          ^Collection next-frames [{:id 99} {:id 100}]
          selections (atom [])]
      (ui/run-now
        (ui/observe-selection call-stack
                              (fn [_ frames]
                                (swap! selections conj [(first frames) (#'debug-view/current-stack-frame view)])))
        (.setAll (.getItems call-stack) initial-frames)
        (ui/select! call-stack {:id 42})
        ;; A fast step can stop again before the UI clears the previous stack.
        (.setAll (.getItems call-stack) next-frames)
        (ui/select! call-stack {:id 99})
        (ui/select! call-stack {:id 100}))
      (is (pos? (count @selections)))
      (doseq [[notified selected] @selections]
        (is (= notified selected))))))

;; Verify each step requests variables with the new frame ID and displays them
;; immediately, guarding against Invalid frameId errors and an empty Variables view.
(deftest stepping-refreshes-frame-variables-test
  (test-support/with-clean-system
    (let [session
          (agent {:status :suspended
                  :generation 0
                  :threadId 7})
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (debugger-variables/make-view!))
          view
          (g/make-node! debug-view/DebugView
            :debug-session session
            :call-stack-view call-stack
            :variables-view variables)
          work (atom [])
          requests (atom [])
          errors (atom [])]
      (with-redefs [dap/frame-variables
                    (fn [_ snapshot frame-id]
                      (swap! requests conj [snapshot frame-id])
                      (when-not (= (:generation snapshot) frame-id)
                        (throw (IllegalArgumentException. "Invalid frameId")))
                      [{:name "count"
                        :value (str frame-id)
                        :variablesReference 0}])
                    console/append-console-entry!
                    (fn [type text] (swap! errors conj [type text]))]
        (ui/run-now
          (ui/observe-selection call-stack
                                (fn [_ _]
                                  (when-let [task (#'debug-view/load-frame-variables! view)]
                                    (swap! work conj task)))))
        (doseq [frame-id [42 99 100]]
          (reset! work [])
          (reset! requests [])
          (ui/run-now
            ;; Keep the old stack displayed until the next stop is rendered.
            (await (send-via future/io-executor session assoc :generation frame-id))
            (g/set-property! view :suspension-state {:stack [{:id frame-id}]})
            (g/node-value view :update-call-stack))
          (doseq [task @work]
            (await! task))
          (ui/run-now
            (is (= [[(dap/suspension @session) frame-id]] @requests))
            (let [^TreeItem item (first (.getChildren (.getRoot variables)))]
              (is (= (str frame-id) (some-> item .getValue :display-value))))))
        (is (= [] @errors))))))

;; Breakpoint edits are submitted directly in UI observation order, even while
;; earlier edits are awaiting the adapter; unchanged edits are not resubmitted.
(deftest breakpoint-edits-do-not-wait-test
  (let [session ::session
        breakpoints (atom #{1})
        requests (atom [])]
    (with-redefs-fn {#'g/node-value (fn [_ _] session)
                     #'ui/ui-disabled? (constantly false)
                     #'ui/->timer (fn [_ _ tick] tick)
                     #'debug-view/collect-enabled-breakpoints (fn [_] @breakpoints)
                     #'debug-view/breakpoints-by-path identity
                     #'dap/set-breakpoints!
                     (fn [_ values]
                       (swap! requests conj values)
                       true)}
      (fn []
        (let [tick (#'debug-view/make-update-timer ::project ::view)]
          (doseq [values [#{1} #{2} #{3} #{3}]]
            (reset! breakpoints values)
            (tick nil nil nil))
          (is (= [#{1} #{2} #{3}] @requests)))))))

;; Verify editor breakpoints become sorted, one-based DAP lines with nonempty
;; conditions preserved and empty conditions omitted.
(deftest breakpoint-conversion-test
  (with-redefs [resource/proj-path :path]
    (is (= {"/main.script" [{:line 1}
                            {:line 6
                             :condition "self.count > 2"}]}
           (#'debug-view/breakpoints-by-path
             #{{:resource {:path "/main.script"}
                :row 5
                :condition "self.count > 2"}
               {:resource {:path "/main.script"}
                :row 0
                :condition ""}})))))

;; Evaluating a table in the console prints its members off the UI thread,
;; guarding against dropping the DAP variablesReference and showing only an address.
(deftest table-evaluation-console-test
  (test-support/with-clean-system
    (let [session
          (agent {:status :suspended
                  :generation 1
                  :threadId 7})

          ^ListView call-stack (ui/run-now (ListView.))
          view
          (g/make-node! debug-view/DebugView
            :debug-session session
            :call-stack-view call-stack)

          entries (atom [])]
      (ui/run-now
        (.add (.getItems call-stack) {:id 42})
        (.select (.getSelectionModel call-stack) (int 0)))

      (with-redefs [dap/request!
                    (fn [_ command arguments]
                      (is (not (ui/on-ui-thread?)))
                      (case command
                        "evaluate"
                        (do
                          (is (= 42 (:frameId arguments)))
                          (is (= "{answer = 42}" (:expression arguments)))
                          {:result "table: result"
                           :variablesReference 1})

                        "variables"
                        {:variables [{:name "answer"
                                      :value "42"
                                      :variablesReference 0}]}))

                    console/append-console-entry!
                    (fn [type text]
                      (swap! entries conj [type text]))]
        (await! (ui/run-now (#'debug-view/on-eval-input view "{answer = 42}")))
        (is (= [[:eval-expression "{answer = 42}"]
                [:eval-result "{ -- table: result"]
                [:eval-result "  answer = 42"]
                [:eval-result "}"]]
               @entries))))))

;; Verify attachment chooses the remote startup module only for remote targets.
(deftest attach-startup-module-test
  (doseq [[target expected-path]
          [[{:process ::process} "/_defold/debugger/start.lua"]
           [{:address "192.168.1.20"} "/_defold/debugger/start_remote.lua"]]]
    (let [calls (atom [])]
      (with-redefs-fn {#'debug-view/built-lua-module
                       (fn [artifacts path]
                         (is (= ::artifacts artifacts))
                         {:path path})
                       #'engine/run-script!
                       (fn [received-target module]
                         (swap! calls conj [:run received-target module]))
                       #'debug-view/start-debugger!
                       (fn [view project received-target stop-on-entry]
                         (swap! calls conj [:connect view project received-target stop-on-entry]))}
        #(debug-view/attach! ::view ::project target ::artifacts))
      (is (= [[:run target
               {:path expected-path}]
              [:connect ::view ::project target true]]
             @calls)))))

;; Verify local connections wait for a discovered port while remote connections
;; use their fixed per-instance port, and both use an explicitly discovered port.
(deftest debugger-port-resolution-test
  (doseq [[target expected-ports]
          [[{:id "local" :process ::process} [nil 49152]]
           [{:id "remote" :instance-index 3} [8175 8175]]
           [{:id "local-known" :process ::process :debugger-port 49152} [49152 49152]]
           [{:id "remote-known" :debugger-port 49152} [49152 49152]]]]
    (test-support/with-clean-system
      (let [current (atom target)
            view (g/make-node! debug-view/DebugView
                   :state-changed-fn (constantly nil))]
        (with-redefs-fn {#'debug-view/collect-enabled-breakpoints (constantly #{})
                         #'project/workspace (constantly ::workspace)
                         #'workspace/project-directory (constantly (io/file "."))
                         #'targets/all-launched-targets
                         (fn []
                           (if (targets/launched-target? target) [@current] []))
                         #'dap/connect!
                         (fn [_ resolve-port _]
                           (let [initial-port (resolve-port)]
                             (swap! current assoc :debugger-port 49152)
                             (is (= expected-ports [initial-port (resolve-port)])))
                           nil)}
          #(debug-view/start-debugger! view ::project target false))))))

;; A delayed close must leave the UI responsive and honor a newer start or detach.
(deftest replacing-debugger-session-test
  (doseq [action [:replace :detach :newer-start :close-error]]
    (testing (name action)
      (test-support/with-clean-system
        (let [old (agent {:status :running})
              started (future/make)
              release (future/make)
              responsive (future/make)
              connections (atom [])
              errors (atom [])
              view
              (g/make-node! debug-view/DebugView
                :debug-session old
                :state-changed-fn (constantly nil))
              on-closed (:on-closed (#'debug-view/make-debugger-callbacks view))]
          (with-redefs-fn {#'debug-view/collect-enabled-breakpoints (constantly #{})
                           #'project/workspace (constantly ::workspace)
                           #'workspace/project-directory (constantly (io/file "."))
                           #'debug-view/show-connect-failed-info!
                           (fn [exception _]
                             (is (ui/on-ui-thread?))
                             (swap! errors conj (ex-message exception)))

                           #'dap/close!
                           (fn [session]
                             (future/complete! started true)
                             (await! release)
                             (if (= :close-error action)
                               (throw (IOException. "Close failed"))
                               (do
                                 (await (send-via future/io-executor session assoc :status :closed))
                                 (ui/run-later (on-closed session @session)))))

                           #'dap/disconnect!
                           (fn [session]
                             (is (not (ui/on-ui-thread?)))
                             (await (send-via future/io-executor session assoc :status :closed))
                             (ui/run-later (on-closed session @session)))

                           #'dap/connect!
                           (fn [_ _ {:keys [target]}]
                             (is (ui/on-ui-thread?))
                             (is (future/done? release))
                             (swap! connections conj target)
                             (agent {:status :running :target target}))}
            (fn []
              (let [work
                    (future/io
                      (await!
                        (ui/run-now
                          (debug-view/start-debugger! view ::project {:id "first"} false))))]
                (try
                  (is (true? (await! started)))
                  (ui/run-later (future/complete! responsive true))
                  (is (= true (deref responsive 10000 ::timeout)))
                  (when (future/done? responsive)
                    (is (= [] @connections))
                    (let [follow-up
                          (case action
                            :detach (ui/run-now (debug-view/detach! view))
                            :newer-start (ui/run-now
                                           (debug-view/start-debugger! view ::project {:id "newer"} true))
                            nil)]
                      (future/complete! release true)
                      (await! work)
                      (when follow-up
                        (await! follow-up))
                      (ui/run-now
                        (is (nil? (g/node-value view :pending-debugger-start)))
                        (is (= (case action
                                 :replace [{:id "first"}]
                                 :newer-start [{:id "newer"}]
                                 [])
                               @connections))
                        (is (= (if (= :close-error action) ["Close failed"] []) @errors)))))
                  (finally
                    (future/complete! release true)
                    (await! work)))))))))))

;; INFO logging suppresses the startup script's print when a listener already
;; exists. Reattachment must still connect using the last discovered port.
(deftest reattach-without-port-announcement-test
  (test-support/with-clean-system
    (let [target
          {:id "engine"
           :process ::process}
          current (atom (assoc target :debugger-port 49152))
          connected-ports (atom [])
          view
          (g/make-node! debug-view/DebugView
            :state-changed-fn (constantly nil))]
      (with-redefs-fn {#'debug-view/built-lua-module (constantly {})
                       #'debug-view/collect-enabled-breakpoints (constantly #{})
                       #'project/workspace (constantly ::workspace)
                       #'workspace/project-directory (constantly (io/file "."))
                       #'targets/all-launched-targets (fn [] [@current])
                       #'targets/update-launched-target!
                       (fn [_ target-info]
                         (swap! current merge target-info))
                       #'engine/run-script! (constantly :ok)
                       #'dap/connect!
                       (fn [_ resolve-port _]
                         (swap! connected-ports conj (resolve-port))
                         nil)}
        (fn []
          (dotimes [_ 2]
            (debug-view/attach! view ::project target ::artifacts))
          (is (= [49152 49152] @connected-ports)))))))

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
