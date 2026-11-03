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
  (:require [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.separator :as fx.separator]
            [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.dialogs :as dialogs]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.eduction :as e])
  (:import [java.io IOException]
           [java.net HttpURLConnection URLEncoder]
           [java.util.concurrent TimeUnit]))

(set! *warn-on-reflection* true)

(defn launch-environment
  "Build the recording environment. The engine resolves xctrace."
  [settings]
  (when (os/is-mac-os?)
    (let [{:keys [path output-directory enabled delay-ms duration-ms include-energy]} (prefs/get settings [:profile :xctrace])]
      (cond-> {"DM_PROFILE_NATIVE_TRACE" (if enabled "1" "0")
               "DM_PROFILE_NATIVE_XCTRACE" (when-not (string/blank? path)
                                             (.getAbsolutePath (io/file path)))
               "DM_PROFILE_NATIVE_TRACE_ENERGY" (if include-energy "1" "0")}
        enabled
        (assoc "DM_PROFILE_NATIVE_TRACE_DELAY_MS" (str delay-ms)
               "DM_PROFILE_NATIVE_TRACE_DURATION_MS" (str duration-ms)
               "DM_PROFILE_NATIVE_TRACE_PATH" (when-not (string/blank? output-directory)
                                                (str output-directory "/")))))))

(defn- resolve-executable!
  "Resolve an executable or ask the selected Xcode. Call off the UI thread."
  [path]
  (when (os/is-mac-os?)
    (let [path (if-not (string/blank? path)
                 path
                 (try
                   (let [child (process/start! {:err :discard} "/usr/bin/xcrun" "--find" "xctrace")]
                     (try
                       (when (and (.waitFor child 5 TimeUnit/SECONDS)
                                  (zero? (.exitValue child)))
                         (process/capture! (process/out child)))
                       (finally
                         (.destroyForcibly child)
                         (.close (process/out child)))))
                   (catch IOException _ nil)))]
      (when path
        (let [file (io/file path)]
          (when (and (.isFile file) (.canExecute file))
            (.getAbsolutePath file)))))))

(defn wrap-log-sink
  "Forward log lines and open completed traces when enabled."
  [settings log-sink]
  (fn on-engine-log-line! [line]
    (log-sink line)
    (when-let [[_ path] (re-matches #"INFO:(?:ENGINE|PROFILER): Native trace complete: (/.+\.trace)" line)]
      (when (and (os/is-mac-os?) (prefs/get settings [:profile :xctrace :open-in-instruments]))
        (future/io
          (try
            (process/exec! {:err :stdout} "/usr/bin/open" "-a" "Instruments" path)
            (catch Exception e
              (log/warn :message "Could not open trace in Instruments" :path path :exception e))))))))

(defn- start-trace! [target {:keys [delay-ms duration-ms include-energy output-directory]}]
  (let [conn (doto ^HttpURLConnection (.openConnection (io/as-url (str (:url target) "/profiler/trace")))
               (.setRequestProperty "Connection" "close")
               (.setConnectTimeout 2000)
               (.setReadTimeout 2000)
               (.setDoOutput true)
               (.setRequestMethod "POST"))]
    (try
      (.setRequestProperty conn "Content-Type" "application/x-www-form-urlencoded")
      (with-open [out (io/writer (.getOutputStream conn) :encoding "UTF-8")]
        (.write out (cond-> (str "delay_ms=" delay-ms "&duration_ms=" duration-ms "&energy=" (if include-energy "1" "0"))
                      (not (string/blank? output-directory))
                      (str "&path=" (URLEncoder/encode (str output-directory "/") "UTF-8")))))
      (let [status (.getResponseCode conn)]
        (case status
          202 (with-open [in (io/reader (.getInputStream conn) :encoding "UTF-8")]
                {:type :info
                 :message (localization/message "notification.engine-profiler.trace-requested"
                                                {"path" (get (json/read in) "path")})})
          409 {:type :warning
               :message (localization/message "notification.engine-profiler.trace-busy")}
          (404 501) {:type :error
                     :message (localization/message "notification.engine-profiler.trace-unsupported")}
          {:type :error
           :message (localization/message "notification.engine-profiler.trace-request-failed"
                                          {"status" status})}))
      (finally
        (.disconnect conn)))))

(handler/defhandler :run.start-native-trace :global
  (active? [] (os/is-mac-os?))
  (enabled? [prefs] (targets/controllable-target? (targets/selected-target prefs)))
  (run [prefs workspace]
    (let [target (targets/selected-target prefs)
          settings (prefs/get prefs [:profile :xctrace])
          notifications (workspace/notifications workspace)]
      (future/io
        (let [notification
              (try
                (start-trace! target settings)
                (catch Exception exception
                  {:type :error
                   :message (localization/message "notification.engine-profiler.trace-connection-failed"
                                                  {"error" (or (ex-message exception)
                                                               (.getSimpleName (class exception)))})}))]
          (ui/run-now
            (notifications/show! notifications (assoc notification :id ::trace))))))))

(handler/register-menu! ::menu :editor.debug-view/debug-end
  (if (os/is-mac-os?)
    [{:label (localization/message "menu.debug.profiler")
      :children [{:label (localization/message "command.run.start-native-trace")
                  :command :run.start-native-trace}]}]
    []))

(defn- resolve-xctrace! [path swap-resolution]
  (let [job (future
              (let [executable (resolve-executable! path)]
                (swap-resolution
                  (fn [resolution]
                    (cond-> resolution
                      (= path (:path resolution))
                      (assoc :executable executable))))))]
    (fn cancel-xctrace-resolution! []
      (future-cancel job))))

(ui/defc prefs-view
  {:compose [{:fx/type fx/ext-state
              :initial-state {:path (prefs/get (:prefs-state props) (:prefs props) [:profile :xctrace :path])}
              :key :resolution
              :swap-key :swap-resolution}
             {:fx/type fx/ext-effect
              :fn resolve-xctrace!
              :args [(:path (:resolution props)) (:swap-resolution props)]}]}
  [{:keys [prefs prefs-state localization-state resolution]}]
  (let [settings (prefs/get prefs-state prefs [:profile :xctrace])
        resolved (contains? resolution :executable)
        executable (:executable resolution)]
    {:fx/type fxui/vertical
     :padding :medium
     :spacing :medium
     :children
     [{:fx/type fxui/label
       :text (localization-state (localization/message "prefs.profile.macos.header"))}
      {:fx/type fx.separator/lifecycle}
      {:fx/type fxui/label
       :text (localization-state (localization/message "prefs.profile.xctrace.header"))}
      {:fx/type fxui/paragraph
       :text (localization-state
               (localization/message
                 (cond
                   (not resolved) "prefs.profile.xctrace.searching"
                   executable "prefs.profile.xctrace.ready"
                   :else "prefs.profile.xctrace.missing")))}
      {:fx/type fxui/grid
       :spacing :medium
       :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                            {:fx/type fx.column-constraints/lifecycle :hgrow :always}]
       :children
       (vec
         (e/mapcat-indexed
           (fn [row field]
             [{:fx/type fxui/label
               :grid-pane/row row
               :grid-pane/column 0
               :text (localization-state (localization/message (str "prefs.label.profile.xctrace." (name field))))}
              (merge
                {:grid-pane/row row
                 :grid-pane/column 1}
                (case field
                  :path {:fx/type fxui/value-field
                         :value (:path settings)
                         :prompt-text (or executable (localization-state (localization/message "prefs.prompt.profile.xctrace.path")))
                         :on-value-changed #(prefs/set! prefs [:profile :xctrace :path] %)}
                  :output-directory
                  {:fx/type fxui/horizontal
                   :spacing :small
                   :children [{:fx/type fxui/value-field
                               :h-box/hgrow :always
                               :value (:output-directory settings)
                               :prompt-text (localization-state (localization/message "prefs.prompt.profile.xctrace.output-directory"))
                               :tooltip (localization-state (localization/message "prefs.help.profile.xctrace.output-directory"))
                               :to-value #(when (or (string/blank? %) (.isAbsolute (io/file %))) %)
                               :on-value-changed #(prefs/set! prefs [:profile :xctrace :output-directory] %)}
                              {:fx/type fxui/button
                               :text "…"
                               :tooltip (localization-state (localization/message "dialog.directory.title.select"))
                               :on-action (fn choose-output-directory! [event]
                                            (let [current-directory (io/file (:output-directory settings))]
                                              (when-let [directory (dialogs/make-directory-dialog
                                                                     (localization-state (localization/message "dialog.directory.title.select"))
                                                                     (when (.isDirectory current-directory) current-directory)
                                                                     (fxui/event->window event))]
                                                (prefs/set! prefs [:profile :xctrace :output-directory] (.getAbsolutePath directory)))))}]}
                  (:enabled :include-energy :open-in-instruments)
                  {:fx/type fxui/check-box
                   :selected (field settings)
                   :on-selected-changed #(prefs/set! prefs [:profile :xctrace field] %)}
                  {:fx/type fxui/value-field
                   :disable (not (:enabled settings))
                   :value (field settings)
                   :to-value #(when-let [value (parse-long %)]
                                (when (<= 0 value 4294967295)
                                  value))
                   :on-value-changed #(prefs/set! prefs [:profile :xctrace field] %)}))])
           (if executable [:path :output-directory :enabled :delay-ms :duration-ms :include-energy :open-in-instruments] [:path])))}]}))
