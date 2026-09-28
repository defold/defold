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
            [editor.resource :as resource]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [support.test-support :as test-support])
  (:import [java.util Collection]
           [javafx.scene.control ListView TreeItem TreeView]
           [org.luaj.vm2.lib.jse JsePlatform]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- await! [value]
  (let [result (deref value 10000 ::timeout)]
    (when (= ::timeout result)
      (throw (IllegalStateException. "Timed out waiting for debugger UI test")))
    result))

;; Verify callbacks from a closed session cannot clear a newer session, while
;; closing the current session clears its debugger state.
(deftest stale-session-callback-test
  (test-support/with-clean-system
    (let [old {:state (atom {:status :closed})}
          current
          {:state (atom {:status :suspended
                         :generation 1})}
          view (g/make-node! debug-view/DebugView
                 :debug-session current
                 :suspension-state {:stack []}
                 :state-changed-fn (constantly nil))
          callbacks (#'debug-view/make-debugger-callbacks view)]
      ((:on-closed callbacks) old)
      ((:on-resumed callbacks) old)
      (ui/run-now
        (is (identical? current (g/node-value view :debug-session)))
        (is (= {:stack []} (g/node-value view :suspension-state))))
      ((:on-closed callbacks) current)
      (ui/run-now
        (is (nil? (g/node-value view :debug-session)))
        (is (nil? (g/node-value view :suspension-state)))))))

;; Verify a stack response received after execution resumes cannot restore the
;; old suspension state in the editor.
(deftest stale-stack-response-test
  (test-support/with-clean-system
    (let [session
          {:state (atom {:status :suspended
                         :generation 1
                         :thread-id 7})}
          view (g/make-node! debug-view/DebugView
                 :debug-session session
                 :state-changed-fn (constantly nil))
          started (promise)
          response (promise)]
      (with-redefs [dap/stack
                    (fn [_ _]
                      (deliver started true)
                      (await! response))]
        (let [work (#'debug-view/update-suspension-state! view session)]
          (await! started)
          (swap! (:state session) assoc :status :running :generation 2)
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
          {:state (atom {:status :suspended
                         :generation 1
                         :thread-id 7})}
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (debugger-variables/make-view!))
          view (g/make-node! debug-view/DebugView
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
          {:state (atom {:status :suspended
                         :generation 0
                         :thread-id 7})}
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (debugger-variables/make-view!))
          view (g/make-node! debug-view/DebugView
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
            (swap! (:state session) assoc :generation frame-id)
            (g/set-property! view :suspension-state {:stack [{:id frame-id}]})
            (g/node-value view :update-call-stack))
          (doseq [task @work]
            (await! task))
          (ui/run-now
            (is (= [[(dap/suspension session) frame-id]] @requests))
            (let [^TreeItem item (first (.getChildren (.getRoot variables)))]
              (is (= (str frame-id) (some-> item .getValue :display-value))))))
        (is (= [] @errors))))))

(deftest breakpoint-updates-are-serialized-test
  (let [session ::session
        breakpoints (atom #{1})
        started (promise)
        release (promise)
        latest (promise)
        requests (atom [])]
    (with-redefs-fn {#'g/node-value (fn [_ _] session)
                     #'ui/ui-disabled? (constantly false)
                     #'ui/->timer (fn [_ _ tick] tick)
                     #'debug-view/collect-enabled-breakpoints (fn [_] @breakpoints)
                     #'debug-view/breakpoints-by-path identity
                     #'dap/set-breakpoints!
                     (fn [_ values]
                       (swap! requests conj values)
                       (if-not (= #{1} values)
                         (deliver latest true)
                         (do
                           (deliver started true)
                           (await! release))))}
      (fn []
        (let [tick (#'debug-view/make-update-timer ::project ::view)]
          (try
            (tick nil nil nil)
            (await! started)
            (reset! breakpoints #{2})
            (tick nil nil nil)
            (reset! breakpoints #{3})
            (tick nil nil nil)
            (is (= [#{1}] @requests))
            (deliver release true)
            (let [deadline (+ (System/nanoTime) 10000000000)]
              (loop []
                (tick nil nil nil)
                (when-not (realized? latest)
                  (when (> (System/nanoTime) deadline)
                    (throw (IllegalStateException. "Timed out waiting for breakpoint update")))
                  (Thread/sleep 10)
                  (recur))))
            (is (= [#{1} #{3}] @requests))
            (finally
              (deliver release true))))))))

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

;; Verify attachment enables local port discovery before running the
;; script, and chooses the remote startup module only for remote targets.
(deftest attach-startup-module-test
  (doseq [[target expected-path]
          [[{:process ::process} "/_defold/debugger/start.lua"]
           [{:address "192.168.1.20"} "/_defold/debugger/start_remote.lua"]]]
    (let [calls (atom [])]
      (with-redefs-fn {#'debug-view/built-lua-module
                       (fn [artifacts path]
                         (is (= ::artifacts artifacts))
                         {:path path})
                       #'targets/update-launched-target!
                       (fn [received-target target-info]
                         (swap! calls conj [:port-update received-target target-info]))
                       #'engine/run-script!
                       (fn [received-target module]
                         (swap! calls conj [:run received-target module]))
                       #'debug-view/start-debugger!
                       (fn [view project received-target stop-on-entry]
                         (swap! calls conj [:connect view project received-target stop-on-entry]))}
        #(debug-view/attach! ::view ::project target ::artifacts))
      (is (= (cond-> []
               (targets/launched-target? target)
               (conj [:port-update target
                      {:debugger-port 0
                       :debugger-port-pending true}])

               true
               (conj [:run target
                      {:path expected-path}]
                     [:connect ::view ::project target true]))
             @calls)))))

;; INFO logging suppresses the startup script's print when a listener already
;; exists. Reattachment must still connect using the last discovered port.
(deftest reattach-without-port-announcement-test
  (test-support/with-clean-system
    (let [target
          {:id "engine"
           :process ::process}
          current (atom (assoc target :debugger-port 49152))
          connected-ports (atom [])
          view (g/make-node! debug-view/DebugView
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
                       (fn [_ resolve-port _ _]
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
