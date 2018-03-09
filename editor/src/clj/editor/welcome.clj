(ns editor.welcome
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [schema.core :as s]
            [util.net :as net])
  (:import (org.apache.commons.io FilenameUtils)
           (java.io PushbackReader FileOutputStream File)
           (java.net MalformedURLException URL)
           (java.time Duration Instant LocalDateTime ZoneId)
           (java.time.format DateTimeParseException)
           (java.util.zip ZipInputStream)
           (javafx.animation AnimationTimer)
           (javafx.event Event)
           (javafx.scene Node Scene Parent)
           (javafx.scene.control ToggleGroup RadioButton Label)
           (javafx.scene.image ImageView Image)
           (javafx.scene.layout HBox StackPane VBox Priority)
           (javafx.scene.shape Rectangle)
           (javafx.scene.text Text TextFlow)
           (javafx.stage Stage)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private open-project-directory-prefs-key "open-project-directory")
(defonce ^:private legacy-recent-project-paths-prefs-key "recent-projects")
(defonce ^:private recent-projects-prefs-key "recent-project-entries")

;; -----------------------------------------------------------------------------
;; Welcome config file parsing
;; -----------------------------------------------------------------------------

(def ^:private VisibleString
  (s/constrained s/Str #(not (string/blank? %)) "VisibleString"))

(def ^:private ImageFileName
  (s/constrained s/Str #(some? (re-matches #"[^/\\]+\.(png|svg)" %)) "ImageFileName"))

(def ^:private URLString
  (s/constrained s/Str #(try (URL. %) true (catch MalformedURLException _ false)) "URLString"))

(def ^:private TemplateProject
  {:name VisibleString
   :description VisibleString
   :image ImageFileName
   :zip-url URLString
   (s/optional-key :duration) VisibleString
   (s/optional-key :user-level) VisibleString
   (s/optional-key :skip-root?) s/Bool})

(def ^:private NewProjectSettings
  {:templates [(s/one TemplateProject "TemplateProject")
               TemplateProject]})

(def ^:private WelcomeSettings
  {:new-project NewProjectSettings})

(defn- load-edn [path]
  (with-open [reader (PushbackReader. (io/reader (io/resource path)))]
    (edn/read reader)))

(defn- load-welcome-settings [path]
  (s/validate WelcomeSettings (load-edn path)))

;; -----------------------------------------------------------------------------
;; Recent projects management
;; -----------------------------------------------------------------------------

(defn- read-project-settings [^File project-file]
  (with-open [reader (io/reader project-file)]
    (settings-core/parse-settings reader)))

(defn- try-load-project-title
  ^String [^File project-file]
  (try
    (or (-> project-file
            read-project-settings
            (settings-core/get-setting ["project" "title"])
            not-empty)
        "Unnamed")
    (catch Exception _
      nil)))

(defn- try-parse-instant
  ^Instant [^String timestamp]
  (when (some? timestamp)
    (try
      (Instant/parse timestamp)
      (catch DateTimeParseException _
        nil))))

(def ^:private xform-recent-projects->timestamps-by-path
  (map (fn [{:keys [project-file last-opened]}]
         (assert (instance? Instant last-opened))
         (let [path (.getAbsolutePath ^File project-file)
               timestamp (str last-opened)]
           [path timestamp]))))

(def ^:private xform-timestamps-by-path->recent-projects
  (keep (fn [[path timestamp]]
          (let [file (io/as-file path)
                instant (try-parse-instant timestamp)]
            (when (and (some? instant)
                       (fs/existing-file? file))
              (when-some [title (try-load-project-title file)]
                {:project-file file
                 :last-opened instant
                 :title title}))))))

(def ^:private xform-paths->recent-projects
  (keep (fn [path]
          (let [file (io/as-file path)]
            (when (fs/existing-file? file)
              (when-some [title (try-load-project-title file)]
                (let [instant (Instant/ofEpochMilli (.lastModified file))]
                  {:project-file file
                   :last-opened instant
                   :title title})))))))

(defn- descending-order [a b]
  (compare b a))

(defn- recent-projects
  "Returns a sequence of recently opened projects. Project files that no longer
  exist will be filtered out. If the user has an older preference file that does
  not contain timestamps, we'll fall back on the last modified time of the
  game.project file itself. The projects are returned in descending order with
  the most recently opened project first."
  [prefs]
  (sort-by :last-opened
           descending-order
           (if-some [timestamps-by-path (prefs/get-prefs prefs recent-projects-prefs-key nil)]
             (into [] xform-timestamps-by-path->recent-projects timestamps-by-path)
             (if-some [paths (prefs/get-prefs prefs legacy-recent-project-paths-prefs-key nil)]
               (into [] xform-paths->recent-projects paths)
               []))))

(defn- set-recent-projects!
  "Takes a sequence of recently opened projects, and writes it to preferences in
  a format that can be deserialized using the recent-projects function."
  [prefs recent-projects]
  (let [timestamps-by-path (into {} xform-recent-projects->timestamps-by-path recent-projects)]
    (prefs/set-prefs prefs recent-projects-prefs-key timestamps-by-path)))

;; -----------------------------------------------------------------------------
;; New project creation
;; -----------------------------------------------------------------------------

(defn- download-proj-zip! [url]
  (let [file ^File (fs/create-temp-file! "template-project" ".zip")]
    (with-open [output (FileOutputStream. file)]
      (do
        ;; TODO net/download! accepts a callback for progress reporting, use that.
        ;; The long timeout suggests we need some progress reporting even before d/l starts.
        (let [timeout 2000]
          (net/download! url output :connect-timeout timeout :read-timeout timeout))
        file))))

(defn- expand-proj-zip! [src dst skip-root?]
  (with-open [zip (ZipInputStream. (io/input-stream src))]
    (loop [entry (.getNextEntry zip)]
      (when entry
        (let [parts (cond-> (string/split (FilenameUtils/separatorsToUnix (.getName entry)) #"/")
                            skip-root? (next))
              entry-dst ^File (apply io/file dst parts)]
          (do
            (if (.isDirectory entry)
              (.mkdir entry-dst)
              (with-open [output (FileOutputStream. entry-dst)]
                (io/copy zip output)))
            (recur (.getNextEntry zip))))))))

;; -----------------------------------------------------------------------------
;; Browse field control
;; -----------------------------------------------------------------------------

(defn- on-directory-field-browse-button-click! [dialog-title ^Event event]
  (let [^Node button (.getSource event)
        browse-field (.getParent button)
        text-field (.lookup browse-field "TextField")
        scene (.getScene button)
        window (.getWindow scene)
        initial-directory (some-> text-field ui/text not-empty io/as-file)
        initial-directory (when (some-> initial-directory (.exists)) initial-directory)]
    (when-some [directory-path (ui/choose-directory dialog-title initial-directory window)]
      (ui/text! text-field directory-path))))

(defn- setup-directory-browse-field! [dialog-title ^Parent browse-field]
  (let [button (.lookup browse-field "Button")]
    (ui/on-action! button (partial on-directory-field-browse-button-click! dialog-title))))

(defn- browse-field-file
  ^File [^Node browse-field]
  (when-some [path (not-empty (ui/text (.lookup browse-field "TextField")))]
    (io/as-file path)))

;; -----------------------------------------------------------------------------
;; Home pane
;; -----------------------------------------------------------------------------

(defn- vague-time-expression
  ^String [^Instant instant]
  (let [now (LocalDateTime/now)
        then (LocalDateTime/ofInstant instant (ZoneId/systemDefault))
        elapsed (Duration/between then now)]
    (condp > (.getSeconds elapsed)
      45 "just now"
      89  "a minute ago"
      149 "a few minutes ago"
      899 (str (.toMinutes elapsed) " minutes ago")
      2699 "half an hour ago"
      5399 "an hour ago"
      8999 "a few hours ago"
      64799 (str (.toHours elapsed) " hours ago")
      129599 "a day ago"
      129600 "a few days ago"
      (str (.toLocalDate then)))))

(defn- make-recent-project-entry
  ^Node [{:keys [^File project-file ^Instant last-opened ^String title] :as _recent-project}]
  (assert (instance? File project-file))
  (assert (instance? Instant last-opened))
  (assert (string? (not-empty title)))
  (doto (VBox.)
    (ui/add-style! "recent-project-entry")
    (ui/children! [(doto (Text. title)
                     (ui/add-style! "title"))
                   (doto (HBox.)
                     (ui/children! [(doto (Label. (.getParent project-file))
                                      (HBox/setHgrow Priority/ALWAYS))
                                    (Label. (vague-time-expression last-opened))]))])))

(defn- make-home-pane
  ^Parent [prefs close-dialog-and-open-project!]
  (doto (ui/load-fxml "welcome/home-pane.fxml")
    (ui/with-controls [recent-projects-list open-button]
      (doto recent-projects-list
        (ui/items! (recent-projects prefs))
        (ui/cell-factory! (fn [recent-project]
                            {:graphic (make-recent-project-entry recent-project)}))))))

;; -----------------------------------------------------------------------------
;; New project pane
;; -----------------------------------------------------------------------------

(defn- make-icon-view
  ^Node [image-file-name]
  (let [image-path (str "welcome/images/" image-file-name)]
    (cond
      (string/ends-with? image-path ".png")
      (doto (ImageView.)
        (.setFitWidth 171.0)
        (.setFitHeight 114.0)
        (.setImage (Image. image-path))
        (.setClip (doto (Rectangle. 171.0 114.0)
                    (.setArcWidth 3.0)
                    (.setArcHeight 3.0))))

      (string/ends-with? image-path ".svg")
      (doto (StackPane.)
        (ui/add-style! "svg-icon")
        (ui/add-child! (ui/load-svg-path image-path))))))

(defn- make-description-view
  ^Node [^String name ^String description]
  (doto (VBox.)
    (ui/add-style! "description")
    (ui/children! [(doto (Text. name)
                     (ui/add-style! "header"))
                   (doto (TextFlow.)
                     (ui/add-child! (Text. description)))])))

(defn- make-template-entry
  ^Node [project-template]
  (doto (HBox.)
    (ui/add-style! "template-entry")
    (ui/children! [(make-icon-view (:image project-template))
                   (make-description-view (:name project-template)
                                          (:description project-template))])))

(defn- make-new-project-pane
  ^Parent [welcome-settings close-dialog-and-open-project!]
  (doto (ui/load-fxml "welcome/new-project-pane.fxml")
    (ui/with-controls [template-list title-field location-browse-field submit-button]
      (setup-directory-browse-field! "Select New Project Location" location-browse-field)
      (doto template-list
        (ui/items! (get-in welcome-settings [:new-project :templates]))
        (ui/cell-factory! (fn [project-template]
                            {:graphic (make-template-entry project-template)})))
      (ui/on-action! submit-button
                     (fn [_]
                       (when-some [project-template (first (ui/selection template-list))]
                         (let [project-title (ui/text title-field)
                               project-location (browse-field-file location-browse-field)]
                           (cond
                             (string/blank? project-title)
                             (dialogs/make-message-box "Invalid Project Title"
                                                       "Please enter a title for the new project.")

                             (not (fs/empty-directory? project-location))
                             (dialogs/make-message-box "Invalid Project Location"
                                                       "Please select an empty directory for the new project.")

                             :else
                             (when-some [template-zip-file (download-proj-zip! (:zip-url project-template))]
                               (expand-proj-zip! template-zip-file project-location (:skip-root? project-template))
                               (close-dialog-and-open-project! (io/file project-location "game.project")))))))))))

;; -----------------------------------------------------------------------------
;; Import project pane
;; -----------------------------------------------------------------------------

(defn- make-open-project-pane
  ^Parent []
  (let [pane (ui/load-fxml "welcome/open-project-pane.fxml")]
    pane))

;; -----------------------------------------------------------------------------
;; Welcome dialog
;; -----------------------------------------------------------------------------

(defn- make-pane-button [label pane]
  (doto (RadioButton.)
    (ui/user-data! :pane pane)
    (ui/add-style! "pane-button")
    (ui/text! label)))

(defn- pane-button->pane
  ^Parent [pane-button]
  (ui/user-data pane-button :pane))

(defn- install-pending-update-check! [^Stage stage update-context]
  (let [tick-fn (fn [^AnimationTimer timer _]
                  (when-let [pending (updater/pending-update update-context)]
                    (when (not= pending (system/defold-editor-sha1))
                      (.stop timer) ; we only ask once on the start screen
                      (ui/run-later
                        (when (dialogs/make-pending-update-dialog stage)
                          (when (updater/install-pending-update! update-context (io/file (system/defold-resourcespath)))
                            (updater/restart!)))))))
        timer (ui/->timer 0.1 "pending-update-check" tick-fn)]
    (.setOnShown stage (ui/event-handler event (ui/timer-start! timer)))
    (.setOnHiding stage (ui/event-handler event (ui/timer-stop! timer)))))

(defn show-welcome-dialog! [prefs update-context open-project-fn]
  (let [welcome-settings (load-welcome-settings "welcome/welcome.edn")
        root (ui/load-fxml "welcome/welcome-dialog.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        close-dialog-and-open-project! (fn [^File defold-project-file]
                                         (ui/close! stage)
                                         (when (.isFile defold-project-file)
                                           (prefs/set-prefs prefs open-project-directory-prefs-key (.getParent defold-project-file)))

                                         ;; NOTE (TODO): We load the project in the same class-loader as welcome is loaded from.
                                         ;; In other words, we can't reuse the welcome page and it has to be closed.
                                         ;; We should potentially changed this when we have uberjar support and hence
                                         ;; faster loading.
                                         (open-project-fn (.getAbsolutePath defold-project-file))
                                         nil)
        left-pane ^Parent (.lookup root "#left-pane")
        pane-buttons-toggle-group (ToggleGroup.)
        pane-buttons [(make-pane-button "HOME" (make-home-pane prefs close-dialog-and-open-project!))
                      (make-pane-button "NEW PROJECT" (make-new-project-pane welcome-settings close-dialog-and-open-project!))
                      (make-pane-button "OPEN PROJECT" (make-open-project-pane))]]

    ;; Make ourselves the main stage.
    (ui/set-main-stage stage)

    ;; Install pending update check.
    (when update-context
      (install-pending-update-check! stage update-context))

    ;; Add the pane buttons to the left panel and configure them to toggle between the panes.
    (doseq [^RadioButton pane-button pane-buttons]
      (.setToggleGroup pane-button pane-buttons-toggle-group)
      (ui/add-child! left-pane pane-button))

    (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                (fn [_ _ selected-pane-button]
                  (let [pane (pane-button->pane selected-pane-button)]
                    (ui/children! root [left-pane pane]))))

    ;; Select the home pane button.
    (.selectToggle pane-buttons-toggle-group (first pane-buttons))

    ;; ---------- TEMP HACK, REMOVE! ----------
    ;; Press Cmd-R to reload the view.
    (.setOnKeyPressed root (ui/event-handler event
                             (let [key-event ^javafx.scene.input.KeyEvent event]
                               (when (and (.isShortcutDown key-event)
                                          (= "r" (.getText key-event)))
                                 (ui/close! stage)
                                 (doto ^Stage (show-welcome-dialog! prefs update-context open-project-fn)
                                   (.setX (.getX stage))
                                   (.setY (.getY stage)))))))
    ;; ---------- TEMP HACK, REMOVE! ----------

    ;; Show the dialog.
    (.setScene stage scene)
    (ui/run-later (ui/show! stage)) ; TEMP HACK, REMOVE!
    stage))
