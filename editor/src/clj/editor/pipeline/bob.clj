;; Copyright 2020-2024 The Defold Foundation
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
            [util.http-util :as http-util])
  (:import [com.dynamo.bob Bob Bob$CommandLineOption Bob$CommandLineOption$ArgCount Bob$CommandLineOption$ArgType IProgress TaskResult]
           [com.dynamo.bob.logging LogHelper]
           [java.io File OutputStream PrintStream PrintWriter]
           [java.net URL]
           [java.nio.charset StandardCharsets]
           [java.nio.file Path]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.io.output WriterOutputStream]))

(set! *warn-on-reflection* true)

(def html5-url-prefix "/html5")
(def html5-mime-types {"js" "application/javascript",
                       "json" "application/json",
                       "wasm" "application/wasm",
                       "webmanifest" "application/manifest+json",
                       "xhtml" "application/xhtml+xml",
                       "zip" "application/zip",
                       "aac" "audio/aac",
                       "mp3" "audio/mp3",
                       "m4a" "audio/mp4",
                       "oga" "audio/ogg",
                       "ogg" "audio/ogg",
                       "wav" "audio/wav",
                       "woff" "font/woff",
                       "woff2" "font/woff2",
                       "apng" "image/apng",
                       "avif" "image/avif",
                       "bmp" "image/bmp",
                       "gif" "image/gif",
                       "jpeg" "image/jpeg",
                       "jpg" "image/jpeg",
                       "jfif" "image/jpeg",
                       "pjpeg" "image/jpeg",
                       "pjp" "image/jpeg",
                       "png" "image/png",
                       "svg" "image/svg+xml",
                       "webp" "image/webp",
                       "cur" "image/x-icon",
                       "ico" "image/x-icon",
                       "tif" "image/tif",
                       "tiff" "image/tiff",
                       "css" "text/css",
                       "htm" "text/html",
                       "html" "text/html",
                       "shtml" "text/html",
                       "txt" "text/plain",
                       "xml" "text/xml",
                       "mp4" "video/mp4",
                       "ogv" "video/ogg",
                       "webm" "video/webm",
                       "m4v" "video/x-m4v"})

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
     (setCanceled [_this canceled])
     (subProgress [_this _work-claimed-from-this]
       (->progress render-progress! task-cancelled? msg-stack-atom))
     (beginTask [_this name _steps]
       (error-reporting/catch-all!
         (swap! msg-stack-atom conj name)
         (render-progress! (progress/make-cancellable-indeterminate name))))
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
              [cli-options internal-options] (parse-options (.toPath (workspace/project-path workspace evaluation-context)) options)
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
;; Bundling
;; -----------------------------------------------------------------------------

(defn- generic-bundle-bob-options [prefs project {:keys [variant texture-compression generate-debug-symbols? generate-build-report? publish-live-update-content? bundle-contentless? platform ^File output-directory] :as _bundle-options}]
  (assert (some? output-directory))
  (assert (or (not (.exists output-directory))
              (.isDirectory output-directory)))
  (assert (string? (not-empty platform)))
  (let [build-server-url (native-extensions/get-build-server-url prefs project)
        editor-texture-compression? (prefs/get prefs [:build :texture-compression])
        build-report-path (.getAbsolutePath (io/file output-directory "report.html"))
        bundle-output-path (.getAbsolutePath output-directory)
        defold-sdk-sha1 (or (system/defold-engine-sha1) "")
        strip-executable? (= "release" variant)
        texture-compression? (case texture-compression
                               "enabled" true
                               "disabled" false
                               "editor" editor-texture-compression?)]
    (cond-> {"platform" platform
             "variant" variant

             ;; From AbstractBundleHandler
             (if bundle-contentless? "exclude-archive" "archive") true
             "bundle-output" bundle-output-path

             ;; From BundleGenericHandler
             "build-server" build-server-url
             "build-server-header" (native-extensions/get-build-server-headers prefs)
             "defoldsdk" defold-sdk-sha1

             ;; Bob uses these to set X-Email/X-Auth HTTP headers,
             ;; which fails if they are nil, so use empty string
             ;; instead.
             "email" ""
             "auth" ""}

            strip-executable? (assoc "strip-executable" true)
            texture-compression? (assoc "texture-compression" true)

            ;; From BundleGenericHandler
            generate-debug-symbols? (assoc "with-symbols" true)
            generate-build-report? (assoc "build-report-html" build-report-path)
            publish-live-update-content? (assoc "liveupdate" "yes"))))

(def ^:private android-architecture-option->bob-architecture-string
  {:architecture-32bit? "armv7-android"
   :architecture-64bit? "arm64-android"})

(defn- android-bundle-bob-options [{:keys [^File keystore ^File keystore-pass ^File key-pass bundle-format] :as bundle-options}]
  (let [bob-architectures
        (for [[option-key bob-architecture] android-architecture-option->bob-architecture-string
              :when (bundle-options option-key)]
          bob-architecture)
        bob-options {"architectures" (string/join "," bob-architectures)}]
    (assert (or (and (nil? keystore)
                     (nil? keystore-pass)
                     (nil? key-pass))
                (and (.isFile keystore)
                     (.isFile keystore-pass)
                     (or (nil? key-pass) (.isFile key-pass)))))
    (cond-> bob-options
            bundle-format (assoc "bundle-format" bundle-format)
            keystore (assoc "keystore" (.getAbsolutePath keystore))
            keystore-pass (assoc "keystore-pass" (.getAbsolutePath keystore-pass))
            key-pass (assoc "key-pass" (.getAbsolutePath key-pass)))))

(def ^:private ios-architecture-option->bob-architecture-string
  {:architecture-64bit? "arm64-ios"
   :architecture-simulator? "x86_64-ios"})

(defn- ios-bundle-bob-options [{:keys [code-signing-identity ^File provisioning-profile sign-app?] :as bundle-options}]
  (let [bob-architectures (for [[option-key bob-architecture] ios-architecture-option->bob-architecture-string
                                :when (bundle-options option-key)]
                            bob-architecture)
        bob-options {"architectures" (string/join "," bob-architectures)}]
    (if-not sign-app?
      bob-options
      (do (assert (string? (not-empty code-signing-identity)))
          (assert (some-> provisioning-profile .isFile))
          (let [provisioning-profile-path (.getAbsolutePath provisioning-profile)]
            (assoc bob-options
              "mobileprovisioning" provisioning-profile-path
              "identity" code-signing-identity))))))

(def ^:private html5-architecture-option->bob-architecture-string
  {:architecture-js-web? "js-web"
   :architecture-wasm-web? "wasm-web"})

(defn- html5-bundle-bob-options [{:keys [] :as bundle-options}]
  (let [bob-architectures (for [[option-key bob-architecture] html5-architecture-option->bob-architecture-string
                                :when (bundle-options option-key)]
                            bob-architecture)
        bob-options {"architectures" (string/join "," bob-architectures)}]
    bob-options))

(def ^:private macos-architecture-option->bob-architecture-string
  {:architecture-x86_64? "x86_64-macos"
   :architecture-arm64? "arm64-macos"})

(defn- macos-bundle-bob-options [bundle-options]
  (let [bob-architectures (for [[option-key bob-architecture] macos-architecture-option->bob-architecture-string
                                :when (bundle-options option-key)]
                            bob-architecture)
        bob-options {"architectures" (string/join "," bob-architectures)}]
    bob-options))

(def bundle-bob-commands ["distclean" "resolve" "build" "bundle"])

(defmulti bundle-bob-options (fn [_prefs _project platform _bundle-options] platform))
(defmethod bundle-bob-options :default [_prefs _project platform _bundle-options] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-bob-options :android [prefs project _platform bundle-options] (merge (generic-bundle-bob-options prefs project bundle-options) (android-bundle-bob-options bundle-options)))
(defmethod bundle-bob-options :html5   [prefs project _platform bundle-options] (merge (generic-bundle-bob-options prefs project bundle-options) (html5-bundle-bob-options bundle-options)))
(defmethod bundle-bob-options :ios     [prefs project _platform bundle-options] (merge (generic-bundle-bob-options prefs project bundle-options) (ios-bundle-bob-options bundle-options)))
(defmethod bundle-bob-options :linux   [prefs project _platform bundle-options] (generic-bundle-bob-options prefs project bundle-options))
(defmethod bundle-bob-options :macos   [prefs project _platform bundle-options] (merge (generic-bundle-bob-options prefs project bundle-options) (macos-bundle-bob-options bundle-options)))
(defmethod bundle-bob-options :windows [prefs project _platform bundle-options] (merge (generic-bundle-bob-options prefs project bundle-options) {"architectures" (bundle-options :platform)}))

;; -----------------------------------------------------------------------------
;; Build HTML5
;; -----------------------------------------------------------------------------

(defn- build-html5-output-path
  ([project]
   (let [ws (project/workspace project)
         build-path (workspace/build-html5-path ws)]
     (io/file build-path "__htmlLaunchDir"))))

(def rebuild-html5-bob-commands ["distclean" "resolve" "build" "bundle"])
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
     ;; internal option: can't set using command line, but it is needed to run
     ;; the HTML5 build locally.
     :local-launch true
     "email" ""
     "auth" ""}))

(defn- try-resolve-html5-file
  ^File [project ^String rel-url]
  (let [build-html5-output-path (build-html5-output-path project)
        project-title (project-title project)
        rel-path (subs rel-url (inc (count html5-url-prefix)))
        content-path (.normalize (.toPath (io/file build-html5-output-path project-title)))
        absolute-path (.normalize (.resolve content-path rel-path))]
    (when (.startsWith absolute-path content-path)
      (.toFile absolute-path))))

(defn- handler [project {:keys [url method]}]
  (when (= method "GET")
    (if (or (= html5-url-prefix url)
            (= (str html5-url-prefix "/") url))
      {:code 302
       :headers {"Location" (str html5-url-prefix "/index.html")}}

      (let [url-without-query-params  (.getPath (URL. (str "http://" url)))
            served-file   (try-resolve-html5-file project url-without-query-params)
            extra-headers {"Content-Type" (html5-mime-types
                                            (FilenameUtils/getExtension (clojure.string/lower-case url-without-query-params))
                                            "application/octet-stream")}]
        (cond
          ;; The requested URL is a directory or located outside build-html5-output-path.
          (or (nil? served-file) (.isDirectory served-file))
          http-util/forbidden-response

          (.exists served-file)
          {:code 200
           :headers (merge {"Content-Length" (str (.length served-file))} extra-headers)
           :body served-file}

          :else
          http-util/not-found-response)))))

(defn html5-handler [project req-headers]
  (handler project req-headers))
