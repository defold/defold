(ns editor.boot
  (:require
   [clojure.edn :as edn]
   [clojure.java.io :as io]
   [clojure.pprint :as pprint]
   [clojure.stacktrace :as stack]
   [clojure.tools.cli :as cli]
   [clojure.string :as str]
   [dynamo.graph :as g]
   [editor.code.view :as code-view]
   [editor.dialogs :as dialogs]
   [editor.system :as system]
   [editor.error-reporting :as error-reporting]
   [editor.fs :as fs]
   [editor.import :as import]
   [editor.prefs :as prefs]
   [editor.progress :as progress]
   [editor.ui :as ui]
   [editor.updater :as updater]
   [editor.welcome :as welcome]
   [service.log :as log]
   [util.net :as net])
  (:import
   [com.defold.control ListCell]
   [java.io File FileOutputStream]
   [java.util Arrays]
   [java.util.zip ZipInputStream ZipEntry]
   [java.net URL URLConnection]
   [javax.imageio ImageIO]
   [javafx.scene Scene]
   [javafx.scene.control Button Control Label ListView]
   [javafx.scene.input MouseEvent]
   [javafx.scene.layout VBox]
   [javafx.stage Stage]
   [javafx.animation AnimationTimer]
   [javafx.util Callback]
   [org.apache.commons.io FilenameUtils]))

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


(defn- add-to-recent-projects [prefs project-file]
  (let [recent (->> (prefs/get-prefs prefs "recent-projects" [])
                 (remove #(= % (str project-file)))
                 (cons (str project-file))
                 (take 10))]
    (prefs/set-prefs prefs "recent-projects" recent)))

(defn- make-list-cell [^File file]
  (let [path (.toPath file)
        parent (.getParent path)
        vbox (VBox.)
        project-label (Label. (str (.getFileName parent)))
        path-label (Label. (str (.getParent parent)))
        ^"[Ljavafx.scene.control.Control;" controls (into-array Control [project-label path-label])]
    ; TODO: Should be css stylable
    (.setStyle path-label "-fx-text-fill: grey; -fx-font-size: 10px;")
    (.addAll (.getChildren vbox) controls)
    vbox))

(def ^:const open-project-directory-prefs-key "open-project-directory")

;; TODO: Remove once fully ported to welcome.clj.
(defn open-welcome [prefs update-context cont]
  (let [^VBox root (ui/load-fxml "welcome.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        ^ListView recent-projects (.lookup root "#recent-projects")
        ^Button open-project (.lookup root "#open-project")
        import-project (.lookup root "#import-project")]

    (ui/set-main-stage stage)

    (ui/on-action! import-project (fn [_] (when-let [file-name (import/open-import-dialog prefs)]
                                            (ui/close! stage)
                                            ; See comment above about main and class-loaders
                                            (cont file-name))))

    (.setOnMouseClicked recent-projects (ui/event-handler e (when (= 2 (.getClickCount ^MouseEvent e))
                                                              (when-let [file (-> recent-projects (.getSelectionModel) (.getSelectedItem))]
                                                                (ui/close! stage)
                                                                ; See comment above about main and class-loaders
                                                                (cont (.getAbsolutePath ^File file))))))
    (.setCellFactory recent-projects (reify Callback (call ^ListCell [this view]
                                                       (proxy [ListCell] []
                                                         (updateItem [file empty]
                                                           (let [this ^ListCell this]
                                                             (proxy-super updateItem file empty)
                                                             (if (or empty (nil? file))
                                                               (proxy-super setText nil)
                                                               (proxy-super setGraphic (make-list-cell file)))))))))
    (let [recent (->>
                   (prefs/get-prefs prefs "recent-projects" [])
                   (map io/file)
                   (filter (fn [^File f] (.isFile f)))
                   (into-array File))]
      (.addAll (.getItems recent-projects) ^"[Ljava.io.File;" recent))
    (.setScene stage scene)
    (ui/show! stage)))

(defn- load-namespaces-in-background
  []
  ;; load the namespaces of the project with all the defnode
  ;; creation in the background
  (future
    (require 'editor.boot-open-project)))

(defn- open-project-with-progress-dialog
  [namespace-loader prefs project update-context]
  (ui/modal-progress
   "Loading project" 100
   (fn [render-progress!]
     (let [namespace-progress (progress/make "Loading editor" 1304) ; magic number from reading namespace-counter after load.
           render-namespace-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 2))
           render-project-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 2 1))
           project-file (io/file project)]
       (reset! namespace-progress-reporter #(render-namespace-progress! (% namespace-progress)))
       ;; ensure that namespace loading has completed
       @namespace-loader
       (code-view/initialize! prefs)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-project)) [])
       (add-to-recent-projects prefs project)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'open-project)) [project-file prefs render-project-progress! update-context])
       (reset! namespace-progress-reporter nil)))))

(defn- select-project-from-welcome
  [namespace-loader prefs update-context]
  (ui/run-later
    (welcome/show-welcome-dialog! prefs update-context
                                  (fn [project]
                                    (open-project-with-progress-dialog namespace-loader prefs project update-context)))))

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
        (open-project-with-progress-dialog namespace-loader prefs game-project-path update-context)
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
