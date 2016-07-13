(ns editor.boot
  (:require [clojure.java.io :as io]
            [clojure.stacktrace :as stack]
            [clojure.pprint :as pprint]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.import :as import]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [com.defold.control ListCell]
           [javafx.scene Scene Parent]
           [javafx.scene.control Button Control Label ListView]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout VBox]
           [javafx.stage Stage]
           [javafx.util Callback]
           [java.io File]
           [java.util.concurrent.atomic AtomicReference]))

(set! *warn-on-reflection* true)

(def namespace-counter (atom 0))
(def namespace-progress-reporter (atom nil))

(alter-var-root (var clojure.core/load-lib)
                (fn [f]
                  (fn [prefix lib & options]
                    (swap! namespace-counter inc)
                    (when @namespace-progress-reporter
                      (@namespace-progress-reporter
                       #(assoc %
                               :message (str "Initializing editor " (if prefix
                                                                      (str prefix "." lib)
                                                                      (str lib)))
                               :pos @namespace-counter)))
                    (apply f prefix lib options))))


(defn- add-to-recent-projects [prefs project-file]
  (let [recent (->> (prefs/get-prefs prefs "recent-projects" [])
                 (remove #(= % (str project-file)))
                 (cons (str project-file))
                 (take 3))]
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

(defn open-welcome [prefs cont]
  (let [^VBox root (ui/load-fxml "welcome.fxml")
        stage (Stage.)
        scene (Scene. root)
        ^ListView recent-projects (.lookup root "#recent-projects")
        ^Button open-project (.lookup root "#open-project")
        import-project (.lookup root "#import-project")]
    (ui/set-main-stage stage)
    (ui/on-action! open-project (fn [_] (when-let [file-name (ui/choose-file "Open Project" "Project Files" ["*.project"])]
                                          (ui/close! stage)
                                          ; NOTE (TODO): We load the project in the same class-loader as welcome is loaded from.
                                          ; In other words, we can't reuse the welcome page and it has to be closed.
                                          ; We should potentially changed this when we have uberjar support and hence
                                          ; faster loading.
                                          (cont file-name))))

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
    (.setResizable stage false)
    (ui/show! stage)))

(defn- load-namespaces-in-background
  []
  ;; load the namespaces of the project with all the defnode
  ;; creation in the background
  (future
    (require 'editor.boot-open-project)))

(defn- open-project-with-progress-dialog
  [namespace-loader prefs project]
  (ui/modal-progress
   "Loading project" 100
   (fn [render-progress!]
     (let [progress (atom (progress/make "Loading project" 733))
           project-file (io/file project)]
       (reset! namespace-progress-reporter #(render-progress! (swap! progress %)))
       (render-progress! (swap! progress progress/message "Initializing project"))
       ;; ensure that namespace loading has completed
       @namespace-loader
       (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-project)) [])
       (add-to-recent-projects prefs project)
       (apply (var-get (ns-resolve 'editor.boot-open-project 'open-project)) [project-file prefs render-progress!])
       (reset! namespace-progress-reporter nil)))))

(defn- select-project-from-welcome
  [namespace-loader prefs]
  (ui/run-later
   (open-welcome prefs
                 (fn [project]
                   (open-project-with-progress-dialog namespace-loader prefs project)))))


;; Exception alerting.

;; keep track of last exception so we can alert only on new ones
(def ^:private ^AtomicReference last-exception (AtomicReference.))

(defn new-exception?
  [ex-map]
  (let [last (.getAndSet last-exception ex-map)]
    (not= last ex-map)))

(defn display-exception [ex]
  (let [ex-map (Throwable->map ex)]
    (when (new-exception? ex-map)
      (let [message (with-out-str (clojure.pprint/pprint ex-map))]
        (ui/run-now (dialogs/make-alert-dialog message))))))

(defn main [args]
  ;; note - the default exception handler gets reset each time a new
  ;; project is opened. this _probably_ doesn't cause any issues, just
  ;; don't rely on the identity of the handler.
  (Thread/setDefaultUncaughtExceptionHandler
   (reify Thread$UncaughtExceptionHandler
     (uncaughtException [_ thread exception]
       (log/error :exception exception :msg "uncaught exception")
       (display-exception exception))))
  (let [namespace-loader (load-namespaces-in-background)
        prefs            (prefs/make-prefs "defold")]
    (try
      (if (= (count args) 0)
        (select-project-from-welcome namespace-loader prefs)
        (open-project-with-progress-dialog namespace-loader prefs (first args)))
      (catch Throwable t
        (log/error :exception t)
        (stack/print-stack-trace t)
        (.flush *out*)
        ;; note - i'm not sure System/exit is a good idea here. it
        ;; means that failing to open one project causes the whole
        ;; editor to quit, maybe losing unsaved work in other open
        ;; projects.
        (System/exit -1)))))
