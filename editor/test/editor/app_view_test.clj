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

(ns editor.app-view-test
  (:require [clojure.test :refer :all]
            [editor.app-view :as app-view]
            [editor.console :as console]
            [editor.targets :as targets]))

;; Verify engine metadata updates the launched target even after startup output,
;; guarding against missing a DAP port announced after the old output limit.
(deftest launched-log-sink-port-discovery-test
  (let [target
        {:id "engine"
         :debugger-port 0
         :log-stream ::stream}
        current (atom target)
        updates (atom [])]
    (with-redefs [targets/update-launched-target!
                  (fn [_ target-info]
                    (swap! updates conj target-info)
                    (swap! current merge target-info))
                  console/current-stream? (constantly false)]
      (let [sink (#'app-view/make-launched-log-sink target (constantly nil))]
        (sink "INFO:DLIB: Log server started on port 8002")
        (sink "INFO:ENGINE: Engine service started on port 8001")
        (sink "INFO:DEBUGGER: Lua DAP debugger listening on 127.0.0.1:49152")
        (is (= 49152 (:debugger-port @current)))
        (is (= "http://127.0.0.1:8001" (:url @current)))
        (is (= "8002" (:log-port @current)))
        (let [before @updates]
          (sink (.repeat "x" 5001))
          (sink "ordinary game output")
          (is (= before @updates)))
        (swap! current assoc :debugger-port-pending true)
        (sink "DEBUG:SCRIPT: Lua DAP debugger port: 49153")
        (is (= 49153 (:debugger-port @current)))
        (is (false? (:debugger-port-pending @current)))
        (is (= "http://127.0.0.1:8001" (:url @current)))
        (is (= "8002" (:log-port @current)))
        (sink "INFO:ENGINE: Engine service started on port 8003")
        (is (= "http://127.0.0.1:8003" (:url @current)))
        (is (= 49153 (:debugger-port @current)))))))
