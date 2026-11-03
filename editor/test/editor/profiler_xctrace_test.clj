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

(ns editor.profiler-xctrace-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.engine :as engine]
            [editor.fs :as fs]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.profiler-xctrace :as profiler-xctrace]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [support.test-support :refer [with-clean-system]])
  (:import [com.sun.net.httpserver HttpExchange HttpHandler HttpServer]
           [java.io InputStream]
           [java.net InetSocketAddress]
           [java.util.concurrent CompletableFuture LinkedBlockingQueue TimeUnit]))

(set! *warn-on-reflection* true)

;; Real engine child processes receive the recording preferences, including
;; disabled startup, automatic discovery, infinite duration and special paths.
(deftest launch-recording-test
  (when (os/is-mac-os?)
    (let [directory (fs/create-temp-directory! "xctrace-launch")
          binary (io/file directory "engine with spaces.sh")
          xctrace-path (.getAbsolutePath binary)
          settings (prefs/make :scopes {:global (io/file directory "preferences")
                                        :project (io/file directory "editor_settings")}
                               :schemas [:default])]
      (try
        (spit binary (str "#!/bin/sh\n"
                          "printf '%s\n' \"$DM_PROFILE_NATIVE_TRACE\" \"$DM_PROFILE_NATIVE_XCTRACE\" "
                          "\"$DM_PROFILE_NATIVE_TRACE_DELAY_MS\" \"$DM_PROFILE_NATIVE_TRACE_DURATION_MS\" \"$DM_PROFILE_NATIVE_TRACE_PATH\" \"$DM_PROFILE_NATIVE_TRACE_ENERGY\"\n"))
        (fs/set-executable! binary true)
        (doseq [[enabled path delay duration energy output-directory]
                [[false xctrace-path 0 5000 true ""]
                 [true "" 0 5000 true ""]
                 [true xctrace-path 123 0 false (str directory "/Traces + å&.trace")]]]
          (prefs/set! settings [:profile :xctrace :enabled] enabled)
          (prefs/set! settings [:profile :xctrace :path] path)
          (prefs/set! settings [:profile :xctrace :delay-ms] delay)
          (prefs/set! settings [:profile :xctrace :duration-ms] duration)
          (prefs/set! settings [:profile :xctrace :include-energy] energy)
          (prefs/set! settings [:profile :xctrace :output-directory] output-directory)
          (let [{:keys [process log-stream]} (ui/run-now
                                               (engine/launch! binary directory settings false 0 true
                                                               (profiler-xctrace/launch-environment settings)))]
            (try
              (is (.waitFor ^Process process 5 TimeUnit/SECONDS))
              (when-not (.isAlive ^Process process)
                (with-open [^InputStream log-stream log-stream]
                  (let [values (string/split-lines (process/capture! log-stream))]
                    (if-not enabled
                      (is (= "0" (first values)))
                      (is (= ["1" path (str delay) (str duration)
                              (if (string/blank? output-directory) "" (str output-directory "/")) (if energy "1" "0")]
                             values)))))
                (is (zero? (process/await-exit-code process))))
              (finally (.destroyForcibly ^Process process)))))
        (finally (fs/delete-directory! directory))))))

;; The Debug command uses current preferences and reports accepted, busy and
;; unsupported replies from a real HTTP server without parsing errors as JSON.
(deftest trace-command-test
  (when (os/is-mac-os?)
    (with-clean-system
      (let [directory (fs/create-temp-directory! "xctrace-request")
            settings (prefs/make :scopes {:global (io/file directory "preferences")} :schemas [:default])
            previous-target (targets/selected-target settings)
            workspace (workspace/make-workspace directory {} {} nil)
            notifications (workspace/notifications workspace)
            context (handler/->context :global {:prefs settings :workspace workspace})
            requests (LinkedBlockingQueue.)
            response (atom nil)
            server (HttpServer/create (InetSocketAddress. "127.0.0.1" 0) 0)]
        (.createContext server "/profiler/trace"
                        (reify HttpHandler
                          (handle [_ exchange]
                            (with-open [^HttpExchange exchange exchange]
                              (.add requests {:method (.getRequestMethod exchange)
                                              :query (.getRawQuery (.getRequestURI exchange))
                                              :content-type (.getFirst (.getRequestHeaders exchange) "Content-Type")
                                              :body (slurp (.getRequestBody exchange) :encoding "UTF-8")})
                              (let [{:keys [status body]} @response
                                    bytes (.getBytes ^String body "UTF-8")]
                                (.sendResponseHeaders exchange status (alength bytes))
                                (with-open [out (.getResponseBody exchange)]
                                  (.write out bytes)))))))
        (.start server)
        (try
          (targets/select-target! settings {:id "trace-test" :url (str "http://127.0.0.1:" (.getPort (.getAddress server)))})
          (doseq [[delay duration energy output-directory encoded-path status body type message]
                  [[0 5000 true "/tmp/Traces + å&.trace" "&path=%2Ftmp%2FTraces+%2B+%C3%A5%26.trace%2F"
                    202 "{\"path\":\"/tmp/game trace.trace\"}" :info
                    (localization/message "notification.engine-profiler.trace-requested" {"path" "/tmp/game trace.trace"})]
                   [123 0 false "" "" 409 "Busy" :warning (localization/message "notification.engine-profiler.trace-busy")]
                   [0 5000 false "" "" 501 "Unsupported" :error (localization/message "notification.engine-profiler.trace-unsupported")]
                   [0 5000 false "" "" 404 "Not Found" :error (localization/message "notification.engine-profiler.trace-unsupported")]]]
            (prefs/set! settings [:profile :xctrace :delay-ms] delay)
            (prefs/set! settings [:profile :xctrace :duration-ms] duration)
            (prefs/set! settings [:profile :xctrace :include-energy] energy)
            (prefs/set! settings [:profile :xctrace :output-directory] output-directory)
            (reset! response {:status status :body body})
            (let [completion (ui/run-now
                               (g/with-auto-evaluation-context evaluation-context
                                 (let [active (handler/active :run.start-native-trace [context] nil evaluation-context)]
                                   (is (handler/enabled? active evaluation-context))
                                   (handler/run active))))]
              (.get ^CompletableFuture completion 5 TimeUnit/SECONDS))
            (is (= {:method "POST" :query nil :content-type "application/x-www-form-urlencoded"
                    :body (str "delay_ms=" delay "&duration_ms=" duration "&energy=" (if energy "1" "0") encoded-path)}
                   (.poll requests 5 TimeUnit/SECONDS)))
            (is (= {:type type :message message :id ::profiler-xctrace/trace}
                   (get-in (g/node-value notifications :notifications) [:id->notification ::profiler-xctrace/trace]))))
          (finally
            (.stop server 0)
            (targets/select-target! settings previous-target)
            (fs/delete-directory! directory)))))))
