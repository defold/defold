(ns editor.boot
  (:require
   [clojure.java.io :as io]
   [clojure.stacktrace :as stack]
   [clojure.tools.cli :as cli]
   [editor.code.view :as code-view]
   [editor.dialogs :as dialogs]
   [editor.error-reporting :as error-reporting]
   [editor.prefs :as prefs]
   [editor.progress :as progress]
   [editor.ui :as ui]
   [editor.updater :as updater]
   [editor.welcome :as welcome]
   [service.log :as log])
  (:import
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
  [namespace-loader prefs project update-context newly-created?]
  (ui/modal-progress
   "Loading project" 100
   (fn [render-progress!]
     (let [namespace-progress (progress/make "Loading editor" 1322) ; magic number from reading namespace-counter after load.
           render-namespace-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 0) 2)
           render-project-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 2) 3)
           project-file (io/file project)]
       (reset! namespace-progress-reporter #(render-namespace-progress! (% namespace-progress)))
       ;; ensure that namespace loading has completed
       @namespace-loader
       (code-view/initialize! prefs)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-project)) [])
       (welcome/add-recent-project! prefs project-file)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'open-project)) [project-file prefs render-project-progress! update-context newly-created?])
       (reset! namespace-progress-reporter nil)))))

(defn- select-project-from-welcome
  [namespace-loader prefs update-context]
  (ui/run-later
    (welcome/show-welcome-dialog! prefs update-context
                                  (fn [project newly-created?]
                                    (open-project-with-progress-dialog namespace-loader prefs project update-context newly-created?)))))

(defn notify-user
  [ex-map sentry-id-promise]
  (when (.isShowing (ui/main-stage))
    (ui/run-now
      (dialogs/make-error-dialog ex-map sentry-id-promise))))

(defn disable-imageio-cache!
  []
  ;; Disabling ImageIO cache speeds up reading images from disk significantly
  (ImageIO/setUseCache false))

(def cli-options
  ;; Path to preference file, mainly used for testing
  [["-prefs" "--preferences PATH" "Path to preferences file"]])

(defn main [args]
  (error-reporting/setup-error-reporting! {:notifier {:notify-fn notify-user}
                                           :sentry   {:project-id "97739"
                                                      :key        "9e25fea9bc334227b588829dd60265c1"
                                                      :secret     "f694ef98d47d42cf8bb67ef18a4e9cdb"}})
  (disable-imageio-cache!)
  (let [args (Arrays/asList args)
        opts (cli/parse-opts args cli-options)
        namespace-loader (load-namespaces-in-background)
        prefs (if-let [prefs-path (get-in opts [:options :preferences])]
                (prefs/load-prefs prefs-path)
                (prefs/make-prefs "defold"))
        update-context (:update-context (updater/start!))]
    (try
      (if-let [game-project-path (get-in opts [:arguments 0])]
        (open-project-with-progress-dialog namespace-loader prefs game-project-path update-context false)
        (select-project-from-welcome namespace-loader prefs update-context))
      (catch Throwable t
        (log/error :exception t)
        (stack/print-stack-trace t)
        (.flush *out*)
        ;; note - i'm not sure System/exit is a good idea here. it
        ;; means that failing to open one project causes the whole
        ;; editor to quit, maybe losing unsaved work in other open
        ;; projects.
        (System/exit -1)))))
