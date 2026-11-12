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
  (:require [clojure.string :as string]
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.engine :as engine]
            [editor.engine-profiler]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.profiler-xctrace :as profiler-xctrace]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.http-server :as http-server]
            [util.path :as path])
  (:import [java.io InputStream]
           [java.util.concurrent CompletableFuture LinkedBlockingQueue TimeUnit]))

(set! *warn-on-reflection* true)

;; The engine child receives the xctrace path and startup recording preference.
(deftest launch-recording-test
  (when (os/is-mac-os?)
    (let [directory (fs/create-temp-directory! "xctrace-launch")
          binary (path/of directory "engine with spaces.sh")
          xctrace-path (str (path/absolute binary))
          prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])]
      (try
        (spit binary "#!/bin/sh\nprintf 'trace=%s\\npath=%s\\n' \"$DM_PROFILE_NATIVE_TRACE\" \"$DM_PROFILE_NATIVE_XCTRACE\"\n")
        (fs/set-executable! (.toFile binary) true)
        (prefs/set! prefs [:profile :xctrace :enabled] true)
        (doseq [path ["" xctrace-path]]
          (prefs/set! prefs [:profile :xctrace :path] path)
          (let [{:keys [process log-stream]} (engine/launch! (.toFile binary) directory prefs false 0 true)]
            (try
              (is (.waitFor ^Process process 5 TimeUnit/SECONDS))
              (when-not (.isAlive ^Process process)
                (with-open [^InputStream log-stream log-stream]
                  (is (= ["trace=1" (str "path=" (if (string/blank? path) (System/getenv "DM_PROFILE_NATIVE_XCTRACE") path))]
                         (string/split-lines (process/capture! log-stream)))))
                (is (zero? (process/await-exit-code process))))
              (finally (.destroyForcibly ^Process process)))))
        (finally (fs/delete-directory! directory))))))

;; Request encoding and HTTP error handling work on every host platform.
;; Busy responses return data rather than a notification or another observer.
(deftest trace-request-test
  (let [directory (fs/create-temp-directory! "xctrace-request")
        prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])
        request (atom nil)
        response (atom nil)]
    (try
      (with-open [server (http-server/start!
                           (fn respond-to-trace! [{:keys [method path query headers body]}]
                             (reset! request {:method method :path path :query query
                                              :content-type (get headers "content-type")
                                              :body (slurp body :encoding "UTF-8")})
                             @response)
                           :host "127.0.0.1")]
        (doseq [[delay duration energy output-directory encoded-path status body expected]
                [[0 5000 true "/tmp/Traces + å&.trace" "&path=%2Ftmp%2FTraces+%2B+%C3%A5%26.trace%2F"
                  202 "{\"state\":\"scheduled\",\"path\":\"/tmp/game trace.trace\"}"
                  {:state :scheduled :path "/tmp/game trace.trace"}]
                 [123 0 false "" "" 409 "Busy" {:state :busy}]
                 [0 5000 false "" "" 501 "Unsupported" {:state :unsupported}]
                 [0 5000 false "" "" 404 "Not Found" {:state :unsupported}]
                 [0 5000 false "" "" 202 "{\"state\":\"scheduled\",\"path\":\"\"}" {:state :error :message "Invalid native trace status"}]
                 [0 5000 false "" "" 400 "Invalid path" {:state :error :status 400}]]]
          (prefs/set! prefs [:profile :xctrace :delay-ms] delay)
          (prefs/set! prefs [:profile :xctrace :duration-ms] duration)
          (prefs/set! prefs [:profile :xctrace :include-energy] energy)
          (prefs/set! prefs [:profile :xctrace :output-directory] output-directory)
          (reset! response (http-server/response status body))
          (is (= expected (profiler-xctrace/start-trace! (http-server/url server) prefs)))
          (is (= {:method "POST" :path "/profiler/trace" :query nil :content-type "application/x-www-form-urlencoded"
                  :body (str "delay_ms=" delay "&duration_ms=" duration "&energy=" (if energy "1" "0") encoded-path)}
                 @request))))
      (finally (fs/delete-directory! directory)))))

(defn- await-request! [^LinkedBlockingQueue requests method]
  (let [request (.poll requests 5 TimeUnit/SECONDS)]
    (is (= method (:method request)))
    (or request (throw (ex-info "No trace request" {:method method})))))

(defn- reply! [{:keys [reply]} state path]
  (future/complete! reply (http-server/json-response {:state state :path path :message "Recording failed"})))

(defn- await-completion! [^CompletableFuture completion]
  (.get completion 5 TimeUnit/SECONDS))

;; One capture notification closes or becomes an actionable result. Real HTTP
;; requests signal transitions, including stopping after dismissing the card.
(deftest trace-lifecycle-test
  (doseq [scenario [:complete :cancelled :stopped :stale-stop :error :superseded :dismissed :auto-open :missing-file :busy :unsupported :command]]
    (when (or (not= :command scenario) (os/is-mac-os?))
      (with-clean-system
        (let [directory (fs/create-temp-directory! "xctrace-lifecycle")
              trace-path (str (path/of directory "game.trace"))
              prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])
              previous-target (targets/selected-target prefs)
              workspace (workspace/make-workspace directory {} {} nil)
              notifications (workspace/notifications workspace)
              requests (LinkedBlockingQueue.)
              engine-process (when (= :auto-open scenario)
                               (process/start! {:out :discard :err :discard}
                                               (str (path/of (System/getProperty "java.home") "bin" "java")) "-version"))]
          (try
            (when-not (= :missing-file scenario) (path/create-directories! trace-path))
            (prefs/set! prefs [:profile :xctrace :open-in-instruments] (= :auto-open scenario))
            (with-open [server (http-server/start!
                                 (fn respond-to-capture! [{:keys [method]}]
                                   (if (= "POST" method)
                                     (case scenario
                                       :busy (http-server/response 409 "Busy")
                                       :unsupported (http-server/response 501 "Unsupported")
                                       (http-server/json-response {:state (if (= :stopped scenario) :recording :scheduled) :path trace-path} 202 nil))
                                     (let [reply (future/make)]
                                       (.put requests {:method method :reply reply})
                                       reply)))
                                 :host "127.0.0.1")]
              (let [target (cond-> {:id "trace-test" :url (http-server/url server)}
                             engine-process (assoc :process engine-process))
                    completion (ui/run-now
                                 (if (= :command scenario)
                                   (do
                                     (targets/select-target! prefs target)
                                     (g/with-auto-evaluation-context evaluation-context
                                       (let [context [(handler/->context :global {:prefs prefs :workspace workspace})]
                                             start (handler/active :run.start-xctrace context nil evaluation-context)
                                             stop (handler/active :run.stop-xctrace context nil evaluation-context)]
                                         (is (and start stop))
                                         (is (handler/enabled? start evaluation-context))
                                         (is (not (handler/enabled? stop evaluation-context)))
                                         (handler/run start))))

                                   (profiler-xctrace/capture! target (targets/launched-target? target) prefs workspace)))
                    get-notifications (fn get-notifications []
                                        (into [] (coll/vals (:id->notification (g/node-value notifications :notifications)))))]
                (when-not (#{:busy :unsupported} scenario)
                  (let [poll (await-request! requests "GET")
                        notification (first (get-notifications))
                        action (first (:actions notification))]
                    (is (= (localization/message "notification.engine-profiler.trace-recording")
                           (:message notification)))
                    (is (= (localization/message "notification.engine-profiler.stop-trace") (:message action)))
                    (when (#{:cancelled :stopped :stale-stop :command} scenario)
                      (let [stopping (ui/run-now
                                       (if (not= :command scenario)
                                         ((:on-action action))
                                         (do
                                           (notifications/close! notifications (:id notification))
                                           (targets/select-target! prefs {:id :all-launched-targets})
                                           (g/with-auto-evaluation-context evaluation-context
                                             (let [context [(handler/->context :global {:prefs prefs :workspace workspace})]
                                                   start (handler/active :run.start-xctrace context nil evaluation-context)
                                                   stop (handler/active :run.stop-xctrace context nil evaluation-context)]
                                               (is (not (handler/enabled? start evaluation-context)))
                                               (is (handler/enabled? stop evaluation-context))
                                               (handler/run stop))))))]
                        (reply! (await-request! requests "GET") :recording
                                (if (= :stale-stop scenario) (str trace-path ".new.trace") trace-path))
                        (when-not (= :stale-stop scenario)
                          (reply! (await-request! requests "DELETE")
                                  (if (= :cancelled scenario) :cancelled :stopping) trace-path))
                        (await-completion! stopping)))
                    (if (not= :dismissed scenario)
                      (reply! poll (case scenario :cancelled :cancelled :error :error (:superseded :stale-stop) :recording :complete)
                              (if (#{:superseded :stale-stop} scenario) (str trace-path ".new.trace") trace-path))
                      (do
                        (ui/run-now (notifications/close! notifications (:id notification)))
                        (reply! poll :recording trace-path)
                        (let [poll (await-request! requests "GET")]
                          (is (zero? (count (get-notifications))))
                          (reply! poll :complete trace-path))))))
                (await-completion! completion)
                (is (.isEmpty requests))
                (let [notification (first (get-notifications))]
                  (case scenario
                    (:cancelled :superseded :stale-stop :auto-open :busy)
                    (is (zero? (count (get-notifications))))

                    (:error :unsupported)
                    (do
                      (is (= :error (:type notification)))
                      (is (= (if (= :unsupported scenario)
                               []
                               [(localization/message "notification.engine-profiler.retry-trace")
                                (localization/message "command.app.preferences")])
                             (mapv :message (:actions notification)))))

                    (do
                      (is (= 1 (count (get-notifications))))
                      (is (= (localization/message "notification.engine-profiler.trace-complete")
                             (:message notification)))
                      (is (= (if (= :missing-file scenario)
                               [(localization/message "command.edit.copy-absolute-path")]
                               [(localization/message "notification.engine-profiler.open-trace")
                                (localization/message "notification.engine-profiler.show-trace")])
                             (mapv :message (:actions notification)))))))))
            (finally
              (when engine-process (.destroyForcibly ^Process engine-process))
              (targets/select-target! prefs previous-target)
              (fs/delete-directory! directory))))))))

;; A pending or active capture blocks a second target until it finishes.
(deftest single-capture-per-workspace-test
  (with-clean-system
    (let [directory (fs/create-temp-directory! "xctrace-single")
          prefs (prefs/make :scopes {:global (path/of directory "preferences")} :schemas [:default])
          workspace (workspace/make-workspace directory {} {} nil)
          trace-path (str (path/create-directories! (path/of directory "game.trace")))
          requests (LinkedBlockingQueue.)]
      (try
        (prefs/set! prefs [:profile :xctrace :open-in-instruments] false)
        (with-open [server (http-server/start!
                             (fn respond-to-capture! [{:keys [method]}]
                               (let [reply (future/make)]
                                 (.put requests {:method method :reply reply})
                                 reply))
                             :host "127.0.0.1")]
          (let [url (http-server/url server)
                first-target {:url url}
                second-target {:url (str url "/second")}]
            (let [capture (profiler-xctrace/capture! first-target false prefs workspace)
                  post (await-request! requests "POST")]
              (await-completion! (profiler-xctrace/capture! second-target false prefs workspace))
              (is (.isEmpty requests))
              (reply! post :recording trace-path)
              (let [poll (await-request! requests "GET")]
                (await-completion! (profiler-xctrace/capture! second-target false prefs workspace))
                (is (.isEmpty requests))
                (reply! poll :complete trace-path))
              (await-completion! capture))
            (let [capture (profiler-xctrace/capture! second-target false prefs workspace)]
              (reply! (await-request! requests "POST") :cancelled trace-path)
              (await-completion! capture))))
        (finally (fs/delete-directory! directory))))))
