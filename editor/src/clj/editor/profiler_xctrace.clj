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

(ns editor.profiler-xctrace
  (:require [clojure.data.json :as json]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.console :as console]
            [editor.future :as future]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.http-client :as http]
            [util.path :as path])
  (:import [java.net URI URLEncoder]
           [javafx.scene.input Clipboard ClipboardContent]))

(set! *warn-on-reflection* true)

(defn update-env [env prefs]
  (if-not (os/is-mac-os?)
    env
    (let [{:keys [enabled path delay-ms duration-ms include-energy output-directory]} (prefs/get prefs [:profile :xctrace])]
      (-> env
          (assoc "DM_PROFILE_NATIVE_TRACE" (if enabled "1" "0"))
          (cond-> (not (string/blank? path))
            (assoc "DM_PROFILE_NATIVE_XCTRACE" (str (path/absolute path)))

            enabled
            (assoc "DM_PROFILE_NATIVE_TRACE_DELAY_MS" (str delay-ms)
                   "DM_PROFILE_NATIVE_TRACE_DURATION_MS" (str duration-ms)
                   "DM_PROFILE_NATIVE_TRACE_ENERGY" (if include-energy "1" "0")
                   "DM_PROFILE_NATIVE_TRACE_PATH" (when-not (string/blank? output-directory)
                                                    (str output-directory "/"))))))))

(defn- open-trace! [path]
  (process/exec! {:err :stdout} "/usr/bin/open" "-a" "Instruments" path))

(defn initialize! [prefs]
  (when (os/is-mac-os?)
    (console/register-local-log-listener!
      (fn open-completed-trace! [line]
        (when (prefs/get prefs [:profile :xctrace :open-in-instruments])
          (when-let [[_ path] (re-matches #"INFO:(?:ENGINE|PROFILER): Native trace complete: (/.+\.trace)" line)]
            (future/io
              (try
                (open-trace! path)
                (catch Exception e
                  (log/warn :message "Could not open trace in Instruments" :path path :exception e))))))))))

(defn- trace-status [response]
  (try
    (let [{:keys [status body]} @response]
      (case (long status)
        (200 202)
        (let [{:keys [state path] :as result} (update (json/read-str body :key-fn keyword) :state keyword)]
          (when-not (and (#{:idle :scheduled :starting :recording :stopping :complete :cancelled :error :unsupported} state)
                         (string? path)
                         (or (not (string/blank? path)) (#{:idle :cancelled :error :unsupported} state)))
            (throw (ex-info "Invalid native trace status" {:response result})))
          result)
        409 {:state :busy}
        (404 501) {:state :unsupported}
        {:state :error :status status}))
    (catch Exception exception
      {:state :error :message (or (ex-message exception) (.getSimpleName (class exception)))})))

(defn start-trace! [url prefs]
  (let [{:keys [delay-ms duration-ms include-energy output-directory]} (prefs/get prefs [:profile :xctrace])]
    (trace-status
      (http/request
        (str url "/profiler/trace")
        :method "POST"
        :as :string
        :headers {"Content-Type" "application/x-www-form-urlencoded"}
        :body (cond-> (str "delay_ms=" delay-ms "&duration_ms=" duration-ms "&energy=" (if include-energy "1" "0"))
                (not (string/blank? output-directory))
                (str "&path=" (URLEncoder/encode (str output-directory "/") "UTF-8")))))))

(defn active-capture? [{:keys [state]}]
  (contains? #{:pending :scheduled :starting :recording :stopping} state))

(defn stoppable-capture? [{:keys [state]}]
  (contains? #{:scheduled :starting :recording} state))

(defn- stop-trace! [url path]
  ;; Avoid stopping a capture that has already been replaced.
  (let [trace-url (str url "/profiler/trace")
        status (trace-status (http/request trace-url :as :string))]
    (if-not (and (= path (:path status)) (stoppable-capture? status))
      status
      (trace-status (http/request trace-url :method "DELETE" :as :string)))))

(defn- trace-error-message [{:keys [state status message]}]
  (cond
    (= :unsupported state) (localization/message "notification.engine-profiler.trace-unsupported")
    status (localization/message "notification.engine-profiler.trace-request-failed" {"status" status})
    :else (localization/message "notification.engine-profiler.trace-failed" {"error" message})))

(defn stop-capture! [workspace {:keys [url path] :as capture}]
  (future/io
    (when (stoppable-capture? capture)
      (let [result (stop-trace! url path)]
        (when (#{:error :unsupported} (:state result))
          (ui/run-now
            (let [current (g/node-value workspace :xctrace-capture)]
              (when (and (= url (:url current)) (= path (:path current)) (stoppable-capture? current))
                (notifications/show!
                  (workspace/notifications workspace)
                  {:id ::trace
                   :type :error
                   :message (trace-error-message result)
                   :actions [{:message (localization/message "notification.engine-profiler.stop-trace")
                              :on-action (fn retry-stop! [] (stop-capture! workspace capture))}]})))))))))

(defn- show-folder-action [path]
  {:message (localization/message "notification.engine-profiler.show-trace")
   :on-action (fn show-folder! [] (ui/open-file (.toFile (path/parent path))))})

(defn- open-result! [workspace path]
  (future/io
    (try
      (open-trace! path)
      (catch Exception exception
        (ui/run-now
          (let [capture (g/node-value workspace :xctrace-capture)]
            (when (and (= :complete (:state capture)) (= path (:path capture)))
              (notifications/show!
                (workspace/notifications workspace)
                {:id ::trace
                 :type :error
                 :message (trace-error-message {:message (ex-message exception)})
                 :actions [{:message (localization/message "notification.engine-profiler.open-trace")
                            :on-action (fn retry-open! [] (open-result! workspace path))}
                           (show-folder-action path)]}))))))))

(defn- capture-notification [workspace {:keys [state path local opened retry!] :as capture}]
  (case state
    (:scheduled :starting :recording :stopping)
    {:id ::trace
     :type :info
     :message (localization/message "notification.engine-profiler.trace-recording")
     :actions (if (stoppable-capture? capture)
                [{:message (localization/message "notification.engine-profiler.stop-trace")
                  :on-action (fn stop-recording! [] (stop-capture! workspace capture))}]
                [])}

    :complete
    (when-not opened
      {:id ::trace
       :type :info
       :message (localization/message "notification.engine-profiler.trace-complete")
       :actions (if-not local
                  [{:message (localization/message "command.edit.copy-absolute-path")
                    :on-action (fn copy-trace-path! []
                                 (.setContent (Clipboard/getSystemClipboard)
                                              (doto (ClipboardContent.) (.putString path))))}]
                  [{:message (localization/message "notification.engine-profiler.open-trace")
                    :on-action (fn open-recording! [] (open-result! workspace path))}
                   (show-folder-action path)])})

    (:error :unsupported)
    {:id ::trace
     :type :error
     :message (trace-error-message capture)
     :actions (if-not retry!
                []
                [{:message (localization/message "notification.engine-profiler.retry-trace")
                  :on-action (fn retry-recording! [] (ui/run-later (retry!)))}
                 {:message (localization/message "command.app.preferences")
                  :on-action (fn show-preferences! []
                               (ui/execute-command (ui/contexts (ui/main-scene) true) :app.preferences nil))}])}

    nil))

(defn- publish-capture! [workspace capture render-card]
  (g/transact {:undoable false}
    [(g/set-property workspace :xctrace-capture capture)
     (when render-card
       (let [notifications (workspace/notifications workspace)]
         (if-let [notification (capture-notification workspace capture)]
           (notifications/show notifications notification)
           (notifications/close notifications ::trace))))])
  (when-let [scene (ui/main-scene)]
    (ui/user-data! scene ::ui/refresh-requested? true)))

(defn- update-capture! [workspace url status]
  (let [previous (g/node-value workspace :xctrace-capture)
        capture (when-not (#{:busy :idle :cancelled :superseded} (:state status))
                  (assoc status :url url))]
    (when (not= previous capture)
      (publish-capture! workspace capture
                        (or (= :pending (:state previous))
                            (not (active-capture? capture))
                            (contains? (:id->notification
                                         (g/node-value (workspace/notifications workspace) :notifications))
                                       ::trace))))))

(defn- track-capture! [{:keys [url address local-address] :as target} launched prefs workspace start]
  (if-not (ui/run-now
            (when-not (active-capture? (g/node-value workspace :xctrace-capture))
              (publish-capture! workspace {:state :pending} false)
              true))
    (future/completed nil)
    (future/io
      (let [trace-url (str url "/profiler/trace")
            initial-status (if start
                             (start-trace! url prefs)
                             (trace-status (http/request trace-url :as :string)))
            trace-path (:path initial-status)]
        (loop [status initial-status]
          (let [status (case (:state status)
                         :complete
                         (let [local (and (path/exists? trace-path)
                                          (or launched
                                              (contains? #{"localhost" "127.0.0.1" "[::1]"} (.getHost (URI. url)))
                                              (and local-address (= address local-address))))
                               opened (when (and local (prefs/get prefs [:profile :xctrace :open-in-instruments]))
                                        (try
                                          ;; Opened by the local log listener.
                                          (when-not launched (open-trace! trace-path))
                                          true
                                          (catch Exception exception
                                            (log/warn :message "Could not open trace in Instruments"
                                                      :path trace-path :exception exception)
                                            false)))]
                           (assoc status :local local :opened opened))

                         :error
                         (cond-> status
                           start (assoc :retry! (fn retry-capture! [] (track-capture! target launched prefs workspace true))))

                         status)]
            (ui/run-now (update-capture! workspace url status))
            (when (active-capture? status)
              (Thread/sleep 250)
              (let [status (trace-status (http/request trace-url :as :string))]
                (recur (if (and (not (string/blank? (:path status))) (not= trace-path (:path status)))
                         {:state :superseded}
                         status))))))))))

(defn capture! [target launched prefs workspace]
  (track-capture! target launched prefs workspace true))

(defn observe-launch-capture! [url prefs workspace]
  (track-capture! {:url url} true prefs workspace false))
