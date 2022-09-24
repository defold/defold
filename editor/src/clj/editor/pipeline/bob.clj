;; Copyright 2020-2022 The Defold Foundation
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
  (:require
    [clojure.java.io :as io]
    [clojure.string :as string]
    [dynamo.graph :as g]
    [editor.code.util :as util]
    [editor.defold-project :as project]
    [editor.engine.build-errors :as engine-build-errors]
    [editor.engine.native-extensions :as native-extensions]
    [editor.error-reporting :as error-reporting]
    [editor.progress :as progress]
    [editor.resource :as resource]
    [editor.system :as system]
    [editor.ui :as ui]
    [editor.prefs :as prefs]
    [editor.workspace :as workspace])
  (:import
    [com.dynamo.bob Bob ClassLoaderScanner IProgress IResourceScanner Project TaskResult]
    [com.dynamo.bob.fs DefaultFileSystem]
    [com.dynamo.bob.util PathUtil]
    [java.io File InputStream PrintStream PrintWriter PipedInputStream PipedOutputStream]
    [java.net URI]
    [java.nio.charset StandardCharsets]
    [org.apache.commons.io FilenameUtils]
    [org.apache.commons.io.output WriterOutputStream]))

(try
  (doto (.getDeclaredField Bob "verbose")
    (.setAccessible true)
    (.setBoolean nil true))
  (catch Exception e
    (throw (Exception. "Couldn't make Bob verbose" e))))

(set! *warn-on-reflection* true)

(def skip-dirs #{".git" "build/default" ".internal"})
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
   (->progress render-progress! (constantly false)))
  ([render-progress! task-cancelled?]
   (->progress render-progress! task-cancelled? (atom [])))
  ([render-progress! task-cancelled? msg-stack-atom]
   (assert (ifn? render-progress!))
   (assert (ifn? task-cancelled?))
   (assert (vector? @msg-stack-atom))
   (reify IProgress
     (isCanceled [_this]
       (task-cancelled?))
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

(defn- ->graph-resource-scanner [ws]
  (let [res-map (->> (g/node-value ws :resource-map)
                  (map (fn [[key val]] [(subs key 1) val]))
                  (into {}))]
    (reify IResourceScanner
      (openInputStream ^InputStream [this path]
        (when-let [r (get res-map path)]
          (io/input-stream r)))
      (exists [this path]
        (if-let [r (get res-map path)]
          (resource/exists? r)
          false))
      (isFile [this path]
        (if-let [r (get res-map path)]
          (= (resource/source-type r) :file)
          false))
      (scan [this pattern]
        (let [res (->> res-map
                    (map first)
                    (filter #(PathUtil/wildcardMatch % pattern))
                    set)]
          res)))))

(defn- project-title [project]
  (let [proj-settings (project/settings project)]
    (get proj-settings ["project" "title"] "Unnamed")))

(defn- run-commands! [project evaluation-context ^Project bob-project commands render-progress! task-cancelled?]
  (try
    (let [result (ui/with-progress [render-progress! render-progress!]
                   (.build bob-project (->progress render-progress! task-cancelled?) (into-array String commands)))
          failed-tasks (filter (fn [^TaskResult r] (not (.isOk r))) result)]
      (if (empty? failed-tasks)
        nil
        {:error {:causes (engine-build-errors/failed-tasks-error-causes project evaluation-context failed-tasks)}}))
    (catch Exception e
      {:exception e})))

(def ^:private build-in-progress-atom (atom false))

(defn build-in-progress? []
  @build-in-progress-atom)

(defn- PrintStream-on ^PrintStream [fn]
  (-> fn
      (PrintWriter-on nil)
      (WriterOutputStream. StandardCharsets/UTF_8 1024 true)
      (PrintStream. true StandardCharsets/UTF_8)))

(defn bob-build! [project evaluation-context bob-commands bob-args render-progress! show-build-log-stream! task-cancelled?]
  {:pre [(vector? bob-commands)
         (every? string? bob-commands)
         (map? bob-args)
         (every? (fn [[key val]] (and (string? key) (string? val))) bob-args)
         (ifn? render-progress!)
         (ifn? show-build-log-stream!)
         (ifn? task-cancelled?)]}
  (reset! build-in-progress-atom true)
  (let [prev-out System/out
        prev-err System/err]
    (with-open [log-stream (PipedInputStream.)
                log-stream-writer (PrintWriter. (PipedOutputStream. log-stream) true StandardCharsets/UTF_8)
                build-out (PrintStream-on
                            #(doseq [line (util/split-lines %)]
                               (when (string/starts-with? line "Bob: ")
                                 (.println log-stream-writer (subs line 5)))))
                build-err (PrintStream-on
                            #(doseq [line (util/split-lines %)]
                               (.println log-stream-writer line)))]
      (try
        (show-build-log-stream! log-stream)
        (System/setOut build-out)
        (System/setErr build-err)
        (if (and (some #(= "build" %) bob-commands)
                 (native-extensions/has-extensions? project evaluation-context)
                 (not (native-extensions/supported-platform? (get bob-args "platform"))))
          {:error {:causes (engine-build-errors/unsupported-platform-error-causes project evaluation-context)}}
          (let [ws (project/workspace project evaluation-context)
                proj-path (str (workspace/project-path ws evaluation-context))
                bob-project (Project. (DefaultFileSystem.) proj-path "build/default")]
            (doseq [[key val] bob-args]
              (.setOption bob-project key val))
            (.setOption bob-project "liveupdate" (.option bob-project "liveupdate" "no"))
            (let [scanner (^ClassLoaderScanner Project/createClassLoaderScanner)]
              (doseq [pkg ["com.dynamo.bob" "com.dynamo.bob.pipeline"]]
                (.scan bob-project scanner pkg)))
            (let [deps (workspace/dependencies ws)]
              (when (seq deps)
                (.setLibUrls bob-project (map #(.toURL ^URI %) deps))
                (ui/with-progress [render-progress! render-progress!]
                  (.resolveLibUrls bob-project (->progress render-progress! task-cancelled?)))))
            (.mount bob-project (->graph-resource-scanner ws))
            (.findSources bob-project proj-path skip-dirs)
            (ui/with-progress [render-progress! render-progress!]
              (run-commands! project evaluation-context bob-project bob-commands render-progress! task-cancelled?))))
        (catch Throwable error
          {:exception error})
        (finally
          (reset! build-in-progress-atom false)
          (System/setOut prev-out)
          (System/setErr prev-err))))))

;; -----------------------------------------------------------------------------
;; Bundling
;; -----------------------------------------------------------------------------

(defn- generic-bundle-bob-args [prefs {:keys [variant texture-compression generate-debug-symbols? generate-build-report? publish-live-update-content? platform ^File output-directory] :as _bundle-options}]
  (assert (some? output-directory))
  (assert (or (not (.exists output-directory))
              (.isDirectory output-directory)))
  (assert (string? (not-empty platform)))
  (let [build-server-url (native-extensions/get-build-server-url prefs)
        editor-texture-compression (if (prefs/get-prefs prefs "general-enable-texture-compression" false) "true" "false")
        build-report-path (.getAbsolutePath (io/file output-directory "report.html"))
        bundle-output-path (.getAbsolutePath output-directory)
        defold-sdk-sha1 (or (system/defold-engine-sha1) "")
        strip-executable? (= "release" variant)]
    (cond-> {"platform" platform
             "variant" variant

             ;; From AbstractBundleHandler
             "archive" "true"
             "bundle-output" bundle-output-path
             "texture-compression" (case texture-compression
                                    "enabled" "true"
                                    "disabled" "false"
                                    "editor" editor-texture-compression)

             ;; From BundleGenericHandler
             "build-server" build-server-url
             "defoldsdk" defold-sdk-sha1

             ;; Bob uses these to set X-Email/X-Auth HTTP headers,
             ;; which fails if they are nil, so use empty string
             ;; instead.
             "email" ""
             "auth" ""}

            strip-executable? (assoc "strip-executable" "true")

            ;; From BundleGenericHandler
            generate-debug-symbols? (assoc "with-symbols" "")
            generate-build-report? (assoc "build-report-html" build-report-path)
            publish-live-update-content? (assoc "liveupdate" "true"))))

(def ^:private android-architecture-option->bob-architecture-string
  {:architecture-32bit? "armv7-android"
   :architecture-64bit? "arm64-android"})

(defn- android-bundle-bob-args [{:keys [^File keystore ^File keystore-pass ^File key-pass bundle-format] :as bundle-options}]
  (let [bob-architectures
        (for [[option-key bob-architecture] android-architecture-option->bob-architecture-string
              :when (bundle-options option-key)]
          bob-architecture)
        bob-args {"architectures" (string/join "," bob-architectures)}]
    (assert (or (and (nil? keystore)
                     (nil? keystore-pass)
                     (nil? key-pass))
                (and (.isFile keystore)
                     (.isFile keystore-pass)
                     (or (nil? key-pass) (.isFile key-pass)))))
    (cond-> bob-args
            bundle-format (assoc "bundle-format" bundle-format)
            keystore (assoc "keystore" (.getAbsolutePath keystore))
            keystore-pass (assoc "keystore-pass" (.getAbsolutePath keystore-pass))
            key-pass (assoc "key-pass" (.getAbsolutePath key-pass)))))

(def ^:private ios-architecture-option->bob-architecture-string
  {:architecture-64bit? "arm64-ios"
   :architecture-simulator? "x86_64-ios"})

(defn- ios-bundle-bob-args [{:keys [code-signing-identity ^File provisioning-profile sign-app?] :as bundle-options}]
  (let [bob-architectures (for [[option-key bob-architecture] ios-architecture-option->bob-architecture-string
                                :when (bundle-options option-key)]
                            bob-architecture)
        bob-args {"architectures" (string/join "," bob-architectures)}]
    (if-not sign-app?
      bob-args
      (do (assert (string? (not-empty code-signing-identity)))
          (assert (some-> provisioning-profile .isFile))
          (let [provisioning-profile-path (.getAbsolutePath provisioning-profile)]
            (assoc bob-args
              "mobileprovisioning" provisioning-profile-path
              "identity" code-signing-identity))))))

(def bundle-bob-commands ["distclean" "build" "bundle"])

(defmulti bundle-bob-args (fn [_prefs platform _bundle-options] platform))
(defmethod bundle-bob-args :default [_prefs platform _bundle-options] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-bob-args :android [prefs _platform bundle-options] (merge (generic-bundle-bob-args prefs bundle-options) (android-bundle-bob-args bundle-options)))
(defmethod bundle-bob-args :html5   [prefs _platform bundle-options] (generic-bundle-bob-args prefs bundle-options))
(defmethod bundle-bob-args :ios     [prefs _platform bundle-options] (merge (generic-bundle-bob-args prefs bundle-options) (ios-bundle-bob-args bundle-options)))
(defmethod bundle-bob-args :linux   [prefs _platform bundle-options] (generic-bundle-bob-args prefs bundle-options))
(defmethod bundle-bob-args :macos   [prefs _platform bundle-options] (generic-bundle-bob-args prefs bundle-options))
(defmethod bundle-bob-args :windows [prefs _platform bundle-options] (merge (generic-bundle-bob-args prefs bundle-options) {"architectures" (bundle-options :platform)}))

;; -----------------------------------------------------------------------------
;; Build HTML5
;; -----------------------------------------------------------------------------

(defn- build-html5-output-path [project]
  (let [ws (project/workspace project)
        build-path (workspace/build-path ws)]
    (io/file build-path "__htmlLaunchDir")))

(def build-html5-bob-commands ["distclean" "build" "bundle"])

(defn build-html5-bob-args [project prefs]
  (let [output-path (build-html5-output-path project)
        proj-settings (project/settings project)
        build-server-url (native-extensions/get-build-server-url prefs)
        defold-sdk-sha1 (or (system/defold-engine-sha1) "")
        compress-archive? (get proj-settings ["project" "compress_archive"])]
    (cond-> {"platform" "js-web"
             "variant" "debug"
             "archive" "true"
             "bundle-output" (str output-path)
             "build-server" build-server-url
             "defoldsdk" defold-sdk-sha1
             "local-launch" "true"
             "email" ""
             "auth" ""}
            compress-archive? (assoc "compress" "true"))))

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

      (let [url-without-query-params  (.getPath (java.net.URL. (str "http://" url)))
            served-file   (try-resolve-html5-file project url-without-query-params)
            extra-headers {"Content-Type" (html5-mime-types
                                            (FilenameUtils/getExtension (clojure.string/lower-case url-without-query-params))
                                            "application/octet-stream")}]
        (cond
          ;; The requested URL is a directory or located outside build-html5-output-path.
          (or (nil? served-file) (.isDirectory served-file))
          {:code 403
           :body "Forbidden"}

          (.exists served-file)
          {:code 200
           :headers (merge {"Content-Length" (str (.length served-file))} extra-headers)
           :body served-file}

          :else
          {:code 404
           :body "Not found"})))))

(defn html5-handler [project req-headers]
  (handler project req-headers))
