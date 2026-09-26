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
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.console :as console]
            [editor.debug-view :as debug-view]
            [editor.debugging.dap :as dap]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [support.test-support :as test-support])
  (:import [java.util Collection]
           [javafx.scene.control ListView TreeItem TreeView]))

(set! *warn-on-reflection* true)

(defn- await! [value]
  (let [result (deref value 10000 ::timeout)]
    (when (= ::timeout result)
      (throw (ex-info "Timed out waiting for debugger UI test" {})))
    result))

(deftest stale-session-callback-test
  (test-support/with-clean-system
    (let [old {:state (atom {:status :closed})}
          current {:state (atom {:status :suspended
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

(deftest stale-stack-response-test
  (test-support/with-clean-system
    (let [session {:state (atom {:status :suspended
                                 :generation 1
                                 :thread-id 7})}
          view (g/make-node! debug-view/DebugView
                 :debug-session session
                 :state-changed-fn (constantly nil))
          started (promise)
          response (promise)]
      (with-redefs [dap/stack (fn [_ _]
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

(deftest selected-frame-variables-test
  (test-support/with-clean-system
    (let [session {:state (atom {:status :suspended
                                 :generation 1
                                 :thread-id 7})}
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (TreeView.))
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
      (with-redefs [dap/frame-variables (fn [_ _ frame-id]
                                          (if-not (= 42 frame-id)
                                            [{:name "second"
                                              :value "false"
                                              :variablesReference 0}]
                                            (do
                                              (deliver started true)
                                              (await! response))))]
        (let [first-work (ui/run-now (#'debug-view/load-frame-variables! view))]
          (await! started)
          (let [second-work (ui/run-now
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

(deftest stepping-refreshes-frame-variables-test
  (test-support/with-clean-system
    (let [session {:state (atom {:status :suspended
                                 :generation 0
                                 :thread-id 7})}
          ^ListView call-stack (ui/run-now (ListView.))
          ^TreeView variables (ui/run-now (TreeView.))
          view (g/make-node! debug-view/DebugView
                 :debug-session session
                 :call-stack-view call-stack
                 :variables-view variables)
          work (atom [])
          requests (atom [])
          errors (atom [])]
      (with-redefs [dap/frame-variables (fn [_ snapshot frame-id]
                                          (swap! requests conj [snapshot frame-id])
                                          (when-not (= (:generation snapshot) frame-id)
                                            (throw (ex-info "Invalid frameId" {})))
                                          [{:name "count"
                                            :value (str frame-id)
                                            :variablesReference 0}])
                    console/append-console-entry! (fn [type text] (swap! errors conj [type text]))]
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
