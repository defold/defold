(ns editor.boot
  (:require
   [clojure.java.io :as io]
   [clojure.stacktrace :as stack]
   [clojure.tools.cli :as cli]
   [editor.analytics :as analytics]
   [editor.code.view :as code-view]
   [editor.dialogs :as dialogs]
   [editor.error-reporting :as error-reporting]
   [editor.login :as login]
   [editor.gl :as gl]
   [editor.prefs :as prefs]
   [editor.progress :as progress]
   [editor.system :as system]
   [editor.ui :as ui]
   [editor.updater :as updater]
   [editor.welcome :as welcome]
   [service.log :as log]
   [util.repo :as repo])
  (:import
   [com.defold.editor Shutdown]
   [java.util Arrays]
   [javax.imageio ImageIO]))

(set! *warn-on-reflection* true)

(def namespace-counter (atom 0))
(def namespace-progress-reporter (atom nil))

(alter-var-root (var clojure.core/load-lib)
                (fn [f]
                  (fn [prefix lib & options]
                    (swap! namespace-counter inc)
                    (when @namespace-progress-reporter
                      (@namespace-progress-reporter
                       #(progress/jump %
                                       @namespace-counter
                                       (str "Initializing editor " (if prefix
                                                                     (str prefix "." lib)
                                                                     (str lib))))))
                    (apply f prefix lib options))))

(defn- load-namespaces-in-background
  []
  ;; load the namespaces of the project with all the defnode
  ;; creation in the background
  (future
    (require 'editor.boot-open-project)))

(defn- open-project-with-progress-dialog
  [namespace-loader prefs project dashboard-client updater newly-created?]
  (ui/modal-progress
   "Loading project"
   (fn [render-progress!]
     (let [namespace-progress (progress/make "Loading editor" 1256) ; Magic number from printing namespace-counter after load. Connecting a REPL skews the result!
           render-namespace-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 0) 2)
           render-project-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 2) 3)
           project-file (io/file project)]
       (reset! namespace-progress-reporter #(render-namespace-progress! (% namespace-progress)))
       ;; ensure that namespace loading has completed
       @namespace-loader
       (code-view/initialize! prefs)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-project)) [])
       (welcome/add-recent-project! prefs project-file)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'open-project)) [project-file prefs render-project-progress! dashboard-client updater newly-created?])
       (reset! namespace-progress-reporter nil)))))

(defn- select-project-from-welcome
  [namespace-loader prefs dashboard-client updater]
  (ui/run-later
    (welcome/show-welcome-dialog! prefs dashboard-client updater
                                  (fn [project newly-created?]
                                    (open-project-with-progress-dialog namespace-loader prefs project dashboard-client updater newly-created?)))))

(defn notify-user
  [ex-map sentry-id-promise]
  (when (.isShowing (ui/main-stage))
    (ui/run-now
      (dialogs/make-unexpected-error-dialog ex-map sentry-id-promise))))

(defn- set-sha1-revisions-from-repo! []
  ;; Use the sha1 of the HEAD commit as the editor revision.
  (when (empty? (system/defold-editor-sha1))
    (when-some [editor-sha1 (repo/detect-editor-sha1)]
      (system/set-defold-editor-sha1! editor-sha1)))

  ;; Try to find the engine revision by looking at the VERSION file in the root
  ;; of the Defold repo. On a stable engine branch, the contents of this file
  ;; will correspond to a Git tag. If the tag is present, it will point to the
  ;; engine revision. This is required when building native extensions.
  (when (empty? (system/defold-engine-sha1))
    (when-some [engine-sha1 (repo/detect-engine-sha1)]
      (system/set-defold-engine-sha1! engine-sha1))))

(defn disable-imageio-cache!
  []
  ;; Disabling ImageIO cache speeds up reading images from disk significantly
  (ImageIO/setUseCache false))

(def cli-options
  ;; Path to preference file, mainly used for testing
  [["-prefs" "--preferences PATH" "Path to preferences file"]])

(defn main [args]
  (when (system/defold-dev?)
    (set-sha1-revisions-from-repo!))
  (error-reporting/setup-error-reporting! {:notifier {:notify-fn notify-user}
                                           :sentry   {:project-id "97739"
                                                      :key        "9e25fea9bc334227b588829dd60265c1"
                                                      :secret     "f694ef98d47d42cf8bb67ef18a4e9cdb"}})
  (disable-imageio-cache!)

  (when-let [support-error (gl/gl-support-error)]
    (when (= (dialogs/make-gl-support-error-dialog support-error) :quit)
      (System/exit -1)))

  (let [args (Arrays/asList args)
        opts (cli/parse-opts args cli-options)
        namespace-loader (load-namespaces-in-background)
        prefs (if-let [prefs-path (get-in opts [:options :preferences])]
                (prefs/load-prefs prefs-path)
                (prefs/make-prefs "defold"))
        dashboard-client (login/make-dashboard-client prefs)
        updater (updater/start!)
        analytics-url "https://www.google-analytics.com/batch"
        analytics-send-interval 300
        invalidate-analytics-uid? (not (login/signed-in? dashboard-client))]
    (analytics/start! analytics-url analytics-send-interval invalidate-analytics-uid?)
    (Shutdown/addShutdownAction analytics/shutdown!)
    (try
      (if-let [game-project-path (get-in opts [:arguments 0])]
        (open-project-with-progress-dialog namespace-loader prefs game-project-path dashboard-client updater false)
        (select-project-from-welcome namespace-loader prefs dashboard-client updater))
      (catch Throwable t
        (log/error :exception t)
        (stack/print-stack-trace t)
        (.flush *out*)
        ;; note - i'm not sure System/exit is a good idea here. it
        ;; means that failing to open one project causes the whole
        ;; editor to quit, maybe losing unsaved work in other open
        ;; projects.
        (System/exit -1)))))
