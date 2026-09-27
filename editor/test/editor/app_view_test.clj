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
            [editor.engine :as engine]
            [editor.targets :as targets]))

;; Parse each listener announcement once while a launched target is waiting for
;; its port, including announcements after the startup log window.
(deftest launched-log-sink-port-discovery-test
  (let [target {:id "engine"
                :debugger-port 0
                :log-stream ::stream}
        current (atom target)
        parsed (atom [])
        parse-debugger-port engine/parse-debugger-port
        announcement "INFO:DEBUGGER: Lua DAP debugger listening on 127.0.0.1:49152"
        replacement-announcement "DEBUG:SCRIPT: Lua DAP debugger port: 49153"]
    (with-redefs [targets/all-launched-targets (fn [] [@current])
                  targets/update-launched-target! (fn [_ target-info]
                                                    (swap! current merge target-info))
                  engine/parse-debugger-port (fn [line]
                                               (swap! parsed conj line)
                                               (parse-debugger-port line))
                  console/current-stream? (constantly false)]
      (let [sink (#'app-view/make-launched-log-sink target (constantly nil))]
        (sink announcement)
        (is (= [announcement] @parsed))
        (is (= 49152 (:debugger-port @current)))

        (sink (.repeat "x" 5001))
        (sink announcement)
        (is (= [announcement] @parsed))

        (swap! current assoc :debugger-port 0)
        (sink "ordinary game output")
        (is (= [announcement] @parsed))
        (sink announcement)
        (is (= [announcement announcement] @parsed))
        (is (= 49152 (:debugger-port @current)))

        (sink announcement)
        (is (= [announcement announcement] @parsed))

        (swap! current assoc :debugger-port-pending true)
        (sink "ordinary game output")
        (is (= [announcement announcement] @parsed))
        (sink replacement-announcement)
        (is (= 49153 (:debugger-port @current)))
        (is (false? (:debugger-port-pending @current)))

        (sink replacement-announcement)
        (is (= [announcement announcement replacement-announcement] @parsed))))))
