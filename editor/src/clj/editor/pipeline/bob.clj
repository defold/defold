(ns editor.pipeline.bob
  (:require
    [clojure.java.io :as io]
    [dynamo.graph :as g]
    [editor.defold-project :as project]
    [editor.error-reporting :as error-reporting]
    [editor.engine.native-extensions :as native-extensions]
    [editor.login :as login]
    [editor.prefs :as prefs]
    [editor.resource :as resource]
    [editor.system :as system]
    [editor.ui :as ui]
    [editor.workspace :as workspace])
  (:import
    [com.dynamo.bob ClassLoaderScanner CompileExceptionError IProgress IResourceScanner Project Task TaskResult]
    [com.dynamo.bob.fs DefaultFileSystem IResource]
    [com.dynamo.bob.util PathUtil]
    [java.io File InputStream]
    [java.net URLDecoder]))

(set! *warn-on-reflection* true)

(def skip-dirs #{".git" "build/default" ".internal"})
(def html5-url-prefix "/html5")

;; TODO - this should be fixed with proper progress at some point
(defn ->progress []
  (reify IProgress
    (subProgress [this work]
      (->progress))
    (worked [this amount]
      )
    (beginTask [this name work]
      )
    (done [this]
      )
    (isCanceled [this]
      false)))

(defn ->graph-resource-scanner [ws]
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

(defn- root-task ^Task [^Task task]
  (if-let [parent (.getProductOf task)]
    (recur parent)
    task))

(defn- task->error [project ^TaskResult r]
  (let [task (root-task (.getTask r))
        node-id (when-let [bob-resource ^IResource (first (.getInputs task))]
                  (let [path (str "/" (.getPath bob-resource))]
                    (project/get-resource-node project path)))]
    {:_node-id node-id
     :value (reify clojure.lang.IExceptionInfo
              (getData [this]
                {:line (.getLineNumber r)}))
     :message (.getMessage r)}))

(defn- exc->error [project ^CompileExceptionError e]
  (let [node-id (when-let [bob-resource ^IResource (.getResource e)]
                  (let [path (str "/" (.getPath bob-resource))]
                    (project/get-resource-node project path)))]
    {:_node-id node-id
     :value (reify clojure.lang.IExceptionInfo
              (getData [this]
                {:line (.getLineNumber e)}))
     :message (.getMessage e)}))

(defn- run-commands! [project ^Project bob-project {:keys [render-error!] :as _build-options} commands]
  (try
    (let [progress (->progress)
          result (.build bob-project progress (into-array String commands))
          failed-tasks (filter (fn [^TaskResult r] (not (.isOk r))) result)]
      (if (seq failed-tasks)
        (let [errors {:causes (map (partial task->error project) failed-tasks)}]
          (render-error! errors)
          false)
        true))
    (catch CompileExceptionError error
      (render-error! {:causes [(exc->error project error)]})
      false)))

(defn- bob-build! [project bob-commands bob-args {:keys [clear-errors! render-error!] :as build-options}]
  (assert (vector? bob-commands))
  (assert (every? string? bob-commands))
  (assert (map? bob-args))
  (assert (every? (fn [[key val]] (and (string? key) (string? val))) bob-args))
  (assert (fn? clear-errors!))
  (assert (fn? render-error!))
  (future
    (error-reporting/catch-all!
      (ui/run-later (clear-errors!))
      (let [ws (project/workspace project)
            proj-path (str (workspace/project-path ws))
            bob-project (Project. (DefaultFileSystem.) proj-path "build/default")]
        (doseq [[key val] bob-args]
          (.setOption bob-project key val))
        (.createPublisher bob-project (.hasOption bob-project "liveupdate"))
        (let [scanner (ClassLoaderScanner.)]
          (doseq [pkg ["com.dynamo.bob" "com.dynamo.bob.pipeline"]]
            (.scan bob-project scanner pkg)))
        (let [deps (workspace/dependencies ws)]
          (.setLibUrls bob-project deps)
          (when (seq deps)
            (.resolveLibUrls bob-project (->progress)))
          (.mount bob-project (->graph-resource-scanner ws))
          (.findSources bob-project proj-path skip-dirs)
          (run-commands! project bob-project build-options bob-commands))))))

(defn- boolean? [value]
  (or (false? value) (true? value)))

;; -----------------------------------------------------------------------------
;; Bundling
;; -----------------------------------------------------------------------------

(def ^:private texture-profiles-prefs-key "general-enable-texture-profiles")

(defn- generic-bundle-bob-args [prefs {:keys [release-mode? generate-build-report? publish-live-update-content? platform ^File output-directory] :as _build-options}]
  (assert (some? output-directory))
  (assert (or (not (.exists output-directory))
              (.isDirectory output-directory)))
  (assert (string? (not-empty platform)))
  (let [[email auth] (login/credentials prefs)
        texture-profiles (prefs/get-prefs prefs texture-profiles-prefs-key true)
        build-server-url (native-extensions/get-build-server-url prefs)
        build-report-path (.getAbsolutePath (io/file output-directory "report.html"))
        bundle-output-path (.getAbsolutePath output-directory)
        defold-sdk-sha1 (or (system/defold-sha1) "")]
    (cond-> {"platform" platform

             ;; From AbstractBundleHandler
             "archive" "true"
             "bundle-output" bundle-output-path
             "texture-profiles" (str texture-profiles)

             ;; From BundleGenericHandler
             "build-server" build-server-url
             "defoldsdk" defold-sdk-sha1}

            ;; From BundleGenericHandler
            (not release-mode?) (assoc "debug" "true")
            generate-build-report? (assoc "build-report-html" build-report-path)
            publish-live-update-content? (assoc "liveupdate" "true")

            ;; Our additions
            email (assoc "email" email)
            auth (assoc "auth" auth))))

(defn- android-bundle-bob-args [{:keys [^File certificate ^File private-key] :as _build-options}]
  (assert (or (nil? certificate) (.isFile certificate)))
  (assert (or (nil? private-key) (.isFile private-key)))
  (cond-> {}
          certificate (assoc "certificate" (.getAbsolutePath certificate))
          private-key (assoc "private-key" (.getAbsolutePath private-key))))

(defn- ios-bundle-bob-args [{:keys [code-signing-identity ^File provisioning-profile] :as _build-options}]
  (assert (string? (not-empty code-signing-identity)))
  (assert (some-> provisioning-profile .isFile))
  (let [provisioning-profile-path (.getAbsolutePath provisioning-profile)]
    {"mobileprovisioning" provisioning-profile-path
     "identity" code-signing-identity}))

(defmulti bundle-bob-args (fn [_prefs platform _build-options] platform))
(defmethod bundle-bob-args :default [_prefs platform _build-options] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-bob-args :android [prefs _platform build-options] (merge (generic-bundle-bob-args prefs build-options) (android-bundle-bob-args build-options)))
(defmethod bundle-bob-args :html5   [prefs _platform build-options] (generic-bundle-bob-args prefs build-options))
(defmethod bundle-bob-args :ios     [prefs _platform build-options] (merge (generic-bundle-bob-args prefs build-options) (ios-bundle-bob-args build-options)))
(defmethod bundle-bob-args :linux   [prefs _platform build-options] (generic-bundle-bob-args prefs build-options))
(defmethod bundle-bob-args :macos   [prefs _platform build-options] (generic-bundle-bob-args prefs build-options))
(defmethod bundle-bob-args :windows [prefs _platform build-options] (generic-bundle-bob-args prefs build-options))

(defn bundle! [project prefs platform build-options]
  (let [bob-commands ["distclean" "build" "bundle"]
        bob-args (bundle-bob-args prefs platform build-options)]
    (bob-build! project bob-commands bob-args build-options)))

;; -----------------------------------------------------------------------------
;; Build HTML5
;; -----------------------------------------------------------------------------

(defn- build-html5-output-path [project]
  (let [ws (project/workspace project)
        build-path (workspace/build-path ws)]
    (str build-path "__htmlLaunchDir")))

(defn build-html5! [project prefs build-options]
  (let [output-path (build-html5-output-path project)
        proj-settings (project/settings project)
        compress-archive? (get proj-settings ["project" "compress_archive"])
        [email auth] (login/credentials prefs)
        bob-commands ["distclean" "build" "bundle"]
        bob-args (cond-> {"archive" "true"
                          "platform" "js-web"
                          "bundle-output" output-path
                          "local-launch" "true"}
                         email (assoc "email" email)
                         auth (assoc "auth" auth)
                         compress-archive? (assoc "compress" "true"))]
    (bob-build! project bob-commands bob-args build-options)))

(defn- handler [project {:keys [url method headers]}]
  (if (= method "GET")
    (let [path (-> url
                 (subs (count html5-url-prefix))
                 (URLDecoder/decode "UTF-8"))
          path (format "%s/%s%s" (build-html5-output-path project) (project-title project) path)
          f (io/file path)]
      (if (.exists f)
        (let [length (.length f)
              response {:code 200
                        :response-headers {"Content-Length" (str length)}
                        :body f}]
          response)
        {:code 404}))))

(defn html5-handler [project req-headers]
  (handler project req-headers))
