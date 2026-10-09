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
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.engine :as engine]
            [editor.fs :as fs]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.system.proto System$Reboot]
           [com.sun.net.httpserver HttpExchange HttpHandler HttpServer]
           [java.io InputStream]
           [java.net InetSocketAddress]
           [java.util.concurrent LinkedBlockingQueue]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Verify valid DAP listener ports are discovered and invalid ports are ignored,
;; guarding against losing service metadata while updating the debugger address.
(deftest dap-listener-discovery-test
  (is (= 8172 (engine/debugger-port {})))
  (is (= 8175 (engine/debugger-port {:instance-index 3})))
  (doseq [address ["127.0.0.1" "0.0.0.0" "192.168.1.20"]
          prefix ["INFO:DEBUGGER: " ""]]
    (is (= {:debugger-port 49152}
           (engine/parse-launched-target-info (str prefix "Lua DAP debugger listening on " address ":49152")))))
  (is (= {:debugger-port 49152}
         (engine/parse-launched-target-info "DEBUG:SCRIPT: Lua DAP debugger port: 49152")))
  (doseq [line ["Lua DAP debugger port: 0"
                "Lua DAP debugger port: 65536"
                "Lua DAP debugger port: 9999999999999999999999999999999"
                "Lua DAP debugger listening on 127.0.0.1:0"
                "Lua DAP debugger listening on 127.0.0.1:65536"
                "ordinary game output"]]
    (is (nil? (engine/parse-launched-target-info line))))
  (is (= {:address "127.0.0.1"
          :url "http://127.0.0.1:8001"
          :log-port "8002"
          :debugger-port 49152}
         (engine/parse-launched-target-info
           (str "INFO:DLIB: Log server started on port 8002\n"
                "INFO:ENGINE: Engine service started on port 8001\n"
                "INFO:DEBUGGER: Lua DAP debugger listening on 127.0.0.1:49152\n")))))

;; Real child processes receive debugger arguments only for debug launches.
(deftest debug-launch-test
  (let [directory (fs/create-temp-directory! "debug-launch")
        binary (io/file directory (if (os/is-win32?) "engine.cmd" "engine.sh"))
        settings (prefs/make :scopes {:project (io/file directory "editor_settings")} :schemas [:default])]
    (try
      (spit binary (if (os/is-win32?)
                     "@echo off\r\n:next\r\nif \"%~1\"==\"\" exit /b 0\r\necho %~1\r\nshift\r\ngoto next\r\n"
                     "#!/bin/sh\nprintf '%s\\n' \"$@\"\n"))
      (fs/set-executable! binary true)
      (doseq [[debug instance-index focus expected]
              [[true 3 true ["--config=debugger.enabled=1" "--config=debugger.port=0"
                             "--config=debugger.wait=1" "--config=project.instance_index=3"]]
               [false 0 true []]
               [true 0 false ["--config=debugger.enabled=1" "--config=debugger.port=0"
                              "--config=debugger.wait=1" "--config=display.focus_on_show=0"]]]]
        (let [{:keys [process log-stream]} (engine/launch! binary directory settings debug instance-index focus)]
          (try
            (.waitFor ^Process process)
            (is (zero? (.exitValue ^Process process)))
            (with-open [^InputStream log-stream log-stream]
              (is (= expected
                     (into [] (filter #(re-find #"^--config=(?:debugger\.|project.instance_index=|display.focus_on_show=)" %))
                           (string/split-lines (or (process/capture! log-stream) ""))))))
            (finally (.destroyForcibly ^Process process)))))
      (finally (fs/delete-directory! directory)))))

;; Real HTTP reboot requests preserve debugger, instance, focus and final
;; project arguments.
(deftest debug-reboot-test
  (let [requests (LinkedBlockingQueue.)
        server (HttpServer/create (InetSocketAddress. "127.0.0.1" 0) 0)]
    (.createContext server "/post/@system/reboot"
                    (reify HttpHandler
                      (handle [_ exchange]
                        (with-open [^HttpExchange exchange exchange]
                          (.add requests [(.getRequestMethod exchange)
                                          (with-open [in (.getRequestBody exchange)] (.readAllBytes in))])
                          (.sendResponseHeaders exchange 200 -1)))))
    (.start server)
    (try
      (doseq [[remote debug focus instance-index expected]
              [[false false true 0
                ["--config=resource.uri=http://editor:8000" "http://editor:8000/game.projectc"]]
               [false true true 0
                ["--config=resource.uri=http://editor:8000" "--config=debugger.enabled=1"
                 "--config=debugger.port=8172" "--config=debugger.wait=1" "http://editor:8000/game.projectc"]]
               [true false false 3
                ["--config=resource.uri=http://editor:8000" "--config=project.instance_index=3"
                 "--config=display.focus_on_show=0" "http://editor:8000/game.projectc"]]
               [false true false 3
                ["--config=resource.uri=http://editor:8000" "--config=debugger.enabled=1"
                 "--config=debugger.port=8175" "--config=debugger.wait=1"
                 "--config=project.instance_index=3" "--config=display.focus_on_show=0"
                 "http://editor:8000/game.projectc"]]
               [true true false 3
                ["--config=resource.uri=http://editor:8000" "--config=debugger.enabled=1"
                 "--config=debugger.port=8175" "--config=debugger.wait=1"
                 "--config=debugger.address=0.0.0.0" "--config=project.instance_index=3"
                 "--config=display.focus_on_show=0" "http://editor:8000/game.projectc"]]]]
        (let [target (cond-> {:url (str "http://127.0.0.1:" (.getPort (.getAddress server)))
                              :instance-index instance-index}
                       (not remote) (assoc :process ::process))
              result (engine/reboot! target "http://editor:8000" debug focus)]
          (is (= :ok result))
          (let [[method bytes] (.take requests)
                message (protobuf/bytes->map-without-defaults System$Reboot bytes)
                arguments (into [] (keep message) [:arg1 :arg2 :arg3 :arg4 :arg5 :arg6 :arg7 :arg8])]
            (is (= "POST" method))
            (is (= expected arguments)))))
      (finally (.stop server 0)))))
