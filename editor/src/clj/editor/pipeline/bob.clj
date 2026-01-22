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

(ns editor.pipeline.bob
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.engine.native-extensions :as native-extensions]
            [editor.error-reporting :as error-reporting]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.java :as java]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :as coll]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.path :as path])
  (:import [com.dynamo.bob Bob Bob$CommandLineOption Bob$CommandLineOption$ArgCount Bob$CommandLineOption$ArgType IProgress IProgress$Task TaskResult]
           [com.dynamo.bob.logging LogHelper]
           [java.io File OutputStream PrintStream PrintWriter]
           [java.nio.charset StandardCharsets]
           [java.nio.file Path]
           [org.apache.commons.io.output WriterOutputStream]))

(set! *warn-on-reflection* true)


(defn ->progress
  ([render-progress!]
   (->progress render-progress! fn/constantly-false))
  ([render-progress! task-cancelled?]
   (->progress render-progress! task-cancelled? (atom [])))
  ([render-progress! task-cancelled? msg-stack-atom]
   (assert (ifn? render-progress!))
   (assert (ifn? task-cancelled?))
   (assert (vector? @msg-stack-atom))
   (reify IProgress
     (isCanceled [_this]
       (task-cancelled?))
     (setCanceled [_this _canceled])
     (subProgress [_this _work-claimed-from-this]
       (->progress render-progress! task-cancelled? msg-stack-atom))
     (beginTask [_this task _steps]
       (error-reporting/catch-all!
         (let [message (condp identical? task
                         IProgress$Task/BUNDLING (localization/message "progress.bundling")
                         IProgress$Task/BUILDING_ENGINE (localization/message "progress.building-engine")
                         IProgress$Task/CLEANING_ENGINE (localization/message "progress.cleaning-engine")
                         IProgress$Task/DOWNLOADING_SYMBOLS (localization/message "progress.downloading-symbols")
                         IProgress$Task/TRANSPILING_TO_LUA (localization/message "progress.transpiling-to-lua")
                         IProgress$Task/READING_TASKS (localization/message "progress.reading-tasks")
                         IProgress$Task/BUILDING (localization/message "progress.building")
                         IProgress$Task/CLEANING (localization/message "progress.cleaning")
                         IProgress$Task/GENERATING_REPORT (localization/message "progress.generating-report")
                         IProgress$Task/WORKING (localization/message "progress.working")
                         IProgress$Task/READING_CLASSES (localization/message "progress.reading-classes")
                         IProgress$Task/DOWNLOADING_ARCHIVES (localization/message "progress.downloading-archives")
                         localization/empty-message)]
           (swap! msg-stack-atom conj message)
           (render-progress! (progress/make-cancellable-indeterminate message)))))
     (worked [_this _amount]
       ;; Bob reports misleading progress amounts.
       ;; We report only "busy" and the name of the task.
       nil)
     (done [_this]
       (error-reporting/catch-all!
         (let [msg (peek (swap! msg-stack-atom pop))]
           (render-progress! (if (some? msg)
                               (progress/make-cancellable-indeterminate msg)
                               progress/done))))))))

(defn- project-title [project]
  (let [proj-settings (project/settings project)]
    (get proj-settings ["project" "title"] "Unnamed")))

(defonce ^:private build-in-progress-atom (atom false))

(defn build-in-progress? []
  @build-in-progress-atom)

(defn- PrintStream-on ^PrintStream [fn]
  (-> fn
      (PrintWriter-on nil)
      (WriterOutputStream. StandardCharsets/UTF_8 1024 true)
      (PrintStream. true StandardCharsets/UTF_8)))

(defn- quote-arg-if-needed
  ^String [^String arg-value]
  (if (or (string/includes? arg-value " ") (string/blank? arg-value))
    (str "\"" arg-value "\"")
    arg-value))

(def ^:private long-option->option-config
  (coll/pair-map-by #(.-longOpt ^Bob$CommandLineOption %) (Bob/getCommandLineOptions)))

(defn- long-option? [x]
  (contains? long-option->option-config x))

(defn- parse-options
  "Given bob options map, parses to a tuple of cli args and internal options

  Returns a tuple of 2 elements:
  - cli options: array of strings
  - internal options: either a map of Project options (string->string) or nil"
  [^Path project-root options-map]
  (letfn [(validate-type [^Bob$CommandLineOption config ^Class expected-class value]
            (when-not (instance? expected-class value)
              (throw (IllegalArgumentException. (str (.-longOpt config) " option is expected to be " (.getSimpleName expected-class) ", was " (some-> (class value) .getSimpleName)))))
            value)
          (stringify-value [^Bob$CommandLineOption config value]
            (condp = (.-argType config)
              Bob$CommandLineOption$ArgType/UNSPECIFIED (str value)
              Bob$CommandLineOption$ArgType/ABS_OR_CWD_REL_PATH (str (.normalize (.resolve project-root (str (validate-type config String value)))))))]
    (let [{:keys [undefined cli internal]}
          (util/group-into {} []
                           (fn option-group [e]
                             (let [k (key e)]
                               (cond
                                 (keyword? k) :internal
                                 (long-option? k) :cli
                                 :else :undefined)))
                           identity
                           options-map)]
      (when undefined
        (throw (IllegalArgumentException. (format "Undefined bob option%s: %s"
                                                  (if (< 1 (count undefined)) "s" "")
                                                  (string/join ", " (map key undefined))))))
      [(into []
             (mapcat (fn [[k v]]
                       (let [k-str (str "--" k)
                             ^Bob$CommandLineOption config (long-option->option-config k)]
                         (condp = (.-argCount config)
                           Bob$CommandLineOption$ArgCount/ZERO
                           (if (validate-type config Boolean v) [k-str] [])
                           Bob$CommandLineOption$ArgCount/ONE
                           [k-str (stringify-value config v)]
                           Bob$CommandLineOption$ArgCount/MANY
                           (if (vector? v)
                             (eduction (mapcat (fn [i] [k-str (stringify-value config i)])) v)
                             [k-str (stringify-value config v)])))))
             cli)
       (when internal
         (coll/pair-map-by (comp name key) (comp str val) internal))])))

(defn invoke!
  "Invoke bob, with the same API as Bob's command line

  Args:
    project     the project node id
    options     a map of command line options. The keys are either long CLI
                option names (strings) without the \"--\" prefix, or internal
                options (keywords). When keys are long CLI option names, the
                values are either:
                - scalars like strings, booleans, integers etc.
                - vectors of scalars — will cause the option to be repeated with
                  each argument, useful for e.g. settings or build-server-header
                  options
                Use true for options that don't take args.
                When keys are internal options, the values are simply coerced to
                strings.
    commands    vector of string bob commands, e.g. distclean, clean, build,
                resolve, bundle

  Kv-args (all optional):
    :task-cancelled?       0-arg function that tells if the bob task invocation
                           was cancelled by the editor, used to stop the build
                           process
    :render-progress!      progress render fn
    :evaluation-context    evaluation context used for the invocation
    :log-output-stream     output stream to pipe the logs to, by default will
                           pipe the output to *out*

  See also:
    https://defold.com/manuals/bob/
    (invoke! (dev/project) {\"help\" true} [])"
  [project options commands
   & {:keys [task-cancelled? render-progress! evaluation-context log-output-stream]
      :or {task-cancelled? fn/constantly-false
           render-progress! progress/null-render-progress!}}]
  {:pre [(every? string? commands)
         (map? options)
         (ifn? render-progress!)
         (or (nil? log-output-stream) (instance? OutputStream log-output-stream))
         (ifn? task-cancelled?)]}
  (reset! build-in-progress-atom true)
  (let [;; bob might notify the progress tracker AFTER it's done!
        render-progress! (progress/until-done render-progress!)
        provided-evaluation-context (some? evaluation-context)
        evaluation-context (or evaluation-context (g/make-evaluation-context))
        basis (:basis evaluation-context)
        prev-out System/out
        prev-err System/err
        log-output-stream (or log-output-stream
                              (WriterOutputStream. ^PrintWriter *out* StandardCharsets/UTF_8 1024 true))
        ;; Don't close the writer because we don't own the log output stream,
        ;; hence we must not close it. Instead, we will only flush it in the end
        log-stream-writer (PrintWriter. log-output-stream true StandardCharsets/UTF_8)]
    (try
      (with-open [build-out (PrintStream-on #(doto log-stream-writer (.print %) .flush))
                  build-err (PrintStream-on #(doto log-stream-writer (.print %) .flush))]
        (let [workspace (project/workspace project evaluation-context)
              options (cond-> options
                              (not (contains? options "root"))
                              (assoc "root" ".")
                              (not (contains? options "verbose"))
                              (assoc "verbose" true))
              [cli-options internal-options] (parse-options (.toPath (workspace/project-directory basis workspace)) options)
              cli-args (-> [] (into cli-options) (into commands))]
          (log/info :bob-command (string/join " " (into ["java" "-jar" "bob.jar"] (map quote-arg-if-needed) cli-args)))
          (System/setOut build-out)
          (System/setErr build-err)
          (ui/with-progress [render-progress! render-progress!]
            (let [result (Bob/invoke
                           java/class-loader
                           (->progress render-progress! task-cancelled?)
                           internal-options
                           (into-array String cli-args))]
              (if (.-success result)
                nil
                (let [failed-tasks (filterv #(not (.isOk ^TaskResult %)) (.-taskResults result))]
                  (if (coll/empty? failed-tasks)
                    {:error (g/map->error {:message "Bob failed, see console output for more details" :severity :fatal})}
                    {:error {:causes (engine-build-errors/failed-tasks-error-causes project evaluation-context failed-tasks)}})))))))
      (catch Exception e
        {:exception e})
      (finally
        (reset! build-in-progress-atom false)
        (System/setOut prev-out)
        (System/setErr prev-err)
        (LogHelper/setVerboseLogging false)
        (.flush log-stream-writer)
        (when-not provided-evaluation-context
          (ui/run-later (g/update-cache-from-evaluation-context! evaluation-context)))))))

;; -----------------------------------------------------------------------------
;; Build HTML5
;; -----------------------------------------------------------------------------

(defn- build-html5-output-path
  ([project]
   (let [ws (project/workspace project)
         build-path (workspace/build-html5-path ws)]
     (io/file build-path "__htmlLaunchDir"))))

(def clean-build-html5-bob-commands ["distclean" "resolve" "build" "bundle"])
(def build-html5-bob-commands ["build" "bundle"])

(defn build-html5-bob-options [project prefs]
  (let [output-path (build-html5-output-path project)
        build-server-url (native-extensions/get-build-server-url prefs project)
        defold-sdk-sha1 (or (system/defold-engine-sha1) "")]
    {"platform" "js-web"
     "architectures" "wasm-web"
     "variant" "debug"
     "archive" true
     "output" (subs workspace/build-html5-dir 1)
     "bundle-output" (str output-path)
     "build-server" build-server-url
     "build-server-header" (native-extensions/get-build-server-headers prefs)
     "defoldsdk" defold-sdk-sha1
     "texture-compression" (prefs/get prefs [:build :texture-compression])
     ;; internal option: can't set using command line, but it is needed to run
     ;; the HTML5 build locally.
     :local-launch true
     "email" ""
     "auth" ""}))

(defn routes [project]
  (let [output-path (.toPath ^File (build-html5-output-path project))]
    {"/html5/{*path}"
     {"GET" (bound-fn [{:keys [path-params]}]
              (let [path-str (:path path-params)]
                (if (= "" path-str)
                  (http-server/redirect "/html5/index.html")
                  (let [resource-path (-> output-path
                                          (.resolve ^String (project-title project))
                                          (.resolve ^String path-str)
                                          (.normalize))]
                    (if (and (.startsWith resource-path output-path)
                             (path/file? resource-path))
                      (http-server/response 200 resource-path)
                      http-server/not-found)))))}}))
