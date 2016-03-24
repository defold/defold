(ns editor.boot
  (:require [clojure.java.io :as io]
            [clojure.stacktrace :as stack]
            [dynamo.graph :as g]
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
           [java.io File]))

(set! *warn-on-reflection* true)
(declare main)

(defmacro deferred
   "Loads and runs a function dynamically to defer loading the namespace.
    Usage: \"(deferred clojure.core/+ 1 2 3)\" returns 6.  There's no issue
    calling require multiple times on an ns."
   [fully-qualified-func & args]
   (let [func (symbol (name fully-qualified-func))
         space (symbol (namespace fully-qualified-func))]
     `(do (require '~space)
          (let [v# (ns-resolve '~space '~func)]
            (v# ~@args)))))

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

(defn open-welcome [prefs]
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
                                          (main [file-name]))))

    (ui/on-action! import-project (fn [_] (when-let [file-name (import/open-import-dialog prefs)]
                                            (ui/close! stage)
                                            ; See comment above about main and class-loaders
                                            (main [file-name]))))

    (.setOnMouseClicked recent-projects (ui/event-handler e (when (= 2 (.getClickCount ^MouseEvent e))
                                                              (when-let [file (-> recent-projects (.getSelectionModel) (.getSelectedItem))]
                                                                (ui/close! stage)
                                                                ; See comment above about main and class-loaders
                                                                (main [(.getAbsolutePath ^File file)])))))
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

(def namespaces-loaded (promise))

(defn main [args]
  (Thread/setDefaultUncaughtExceptionHandler
   (reify Thread$UncaughtExceptionHandler
     (uncaughtException [_ thread exception]
       (log/error :exception exception :msg "uncaught exception"))))
  (let [prefs (prefs/make-prefs "defold")]
    (if (= (count args) 0)
      (do
        ;; load the namespaces of the project with all the defnode
        ;; creation in the background while the open-welcome is coming
        ;; up and the user is browsing for a project
        (future ((fn [p]
                   (deferred editor.boot-open-project/load-namespaces)
                   (deliver p true)) namespaces-loaded))
        (ui/run-later (open-welcome prefs)))
      (try
        (ui/modal-progress "Loading project" 100
                           (fn [render-progress!]
                             (do
                               (let [progress  (atom (progress/make "Loading project" 1))]
                                 (render-progress! (swap! progress progress/message "Initializing project"))
                                 ;; ensure the the namespaces havebeen loaded
                                 @namespaces-loaded
                                 (deferred editor.boot-open-project/initialize-project)
                                 (let [project-file (first args)]
                                   (add-to-recent-projects prefs project-file)
                                   (deferred editor.boot-open-project/open-project (io/file project-file) prefs render-progress!))))))
        (catch Throwable t
          (log/error :exception t)
          (stack/print-stack-trace t)
          (.flush *out*)
          (System/exit -1))))))
