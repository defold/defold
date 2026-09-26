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

(ns editor.engine-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.engine :as engine]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.system :as system]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Verify listener discovery accepts valid ports, rejects invalid/unrelated output,
;; preserves engine service metadata, and computes per-instance attach ports.
(deftest dap-listener-discovery-test
  (is (= 8172 (engine/debugger-port {})))
  (is (= 8175 (engine/debugger-port {:instance-index 3})))
  (doseq [address ["127.0.0.1" "0.0.0.0" "192.168.1.20"]]
    (is (= 49152 (engine/parse-debugger-port (str "INFO:DEBUGGER: Lua DAP debugger listening on " address ":49152")))))
  (is (nil? (engine/parse-debugger-port "Lua DAP debugger listening on 127.0.0.1:0")))
  (is (nil? (engine/parse-debugger-port "Lua DAP debugger listening on 127.0.0.1:65536")))
  (is (nil? (engine/parse-debugger-port "ordinary game output")))
  (is (= {:address "127.0.0.1"
          :url "http://127.0.0.1:8001"
          :log-port "8002"
          :debugger-port 49152}
         (engine/parse-launched-target-info
           (str "INFO:DLIB: Log server started on port 8002\n"
                "INFO:ENGINE: Engine service started on port 8001\n"
                "INFO:DEBUGGER: Lua DAP debugger listening on 127.0.0.1:49152\n")))))

;; Verify debug launches enable native DAP with an ephemeral port and startup wait,
;; while ordinary launches omit the debugger arguments.
(deftest debug-launch-test
  (let [launches (atom [])
        binary (.getAbsoluteFile (io/file "dmengine"))]
    (with-redefs [prefs/get (fn [_ path]
                              (case path
                                [:run :engine-arguments] ""
                                [:run :quit-on-escape] false))
                  system/defold-log-dir (constantly nil)
                  process/start! (fn [& args]
                                   (swap! launches conj args)
                                   ::process)
                  process/out (constantly ::stream)]
      (let [target (engine/launch! binary (io/file "/project") nil true 3 true)
            [_ command & args] (peek @launches)]
        (is (= (.getAbsolutePath binary) command))
        (is (= ["--config=debugger.enabled=1"
                "--config=debugger.port=0"
                "--config=debugger.wait=1"
                "--config=project.instance_index=3"] args))
        (is (= 0 (:debugger-port target))))
      (engine/launch! binary (io/file "/project") nil false 0 true)
      (is (= [] (into [] (drop 2) (peek @launches)))))))
