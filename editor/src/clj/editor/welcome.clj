(ns editor.welcome
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.string :as string]
            [editor.analytics :as analytics]
            [editor.client :as client]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.git :as git]
            [editor.jfx :as jfx]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.ui.updater :as ui.updater]
            [schema.core :as s]
            [util.net :as net]
            [util.thread-util :refer [preset!]]
            [util.time :as time])
  (:import [clojure.lang ExceptionInfo]
           [java.io File FileOutputStream PushbackReader]
           [java.net MalformedURLException SocketException SocketTimeoutException URL UnknownHostException]
           [java.time Instant]
           [java.util.zip ZipInputStream]
           [javafx.beans.property SimpleObjectProperty StringProperty]
           [javafx.event Event]
           [javafx.geometry Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button ButtonBase Label ListView ProgressBar RadioButton TextArea TextField ToggleGroup]
           [javafx.scene.image ImageView Image]
           [javafx.scene.input KeyEvent MouseEvent]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [javafx.scene.shape Rectangle]
           [javafx.scene.text Text TextFlow]
           [javax.net.ssl SSLException]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private last-opened-project-directory-prefs-key "open-project-directory")
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

(def ^:private TemplateProjectCategory
  {:label VisibleString
   :templates [(s/one TemplateProject "TemplateProject")
               TemplateProject]})

(def ^:private NewProjectSettings
  {:categories [(s/one TemplateProjectCategory "TemplateProjectCategory")
                TemplateProjectCategory]})

(def ^:private WelcomeSettings
  {:new-project NewProjectSettings})

(defn- load-edn [path]
  (with-open [reader (PushbackReader. (io/reader (io/resource path)))]
    (edn/read reader)))

(defn- load-welcome-settings [path]
  (s/validate WelcomeSettings (load-edn path)))

;; -----------------------------------------------------------------------------
;; Game project settings
;; -----------------------------------------------------------------------------

(defn- read-project-settings [^File project-file]
  (with-open [reader (io/reader project-file)]
    (settings-core/parse-settings reader)))

(defn- try-read-project-title
  ^String [^File project-file]
  (try
    (or (-> project-file
            read-project-settings
            (settings-core/get-setting ["project" "title"])
            not-empty)
        "Unnamed")
    (catch Exception _
      nil)))

(defn- write-project-title! [^File project-file ^String title]
  (assert (string? (not-empty title)))
  (spit project-file
        (-> project-file
            read-project-settings
            (settings-core/set-setting ["project" "title"] title)
            settings-core/settings->str)))

;; -----------------------------------------------------------------------------
;; Preferences management
;; -----------------------------------------------------------------------------

(defn- last-opened-project-directory
  ^File [prefs]
  (when-some [directory (io/as-file (prefs/get-prefs prefs last-opened-project-directory-prefs-key nil))]
    directory))

(defn- set-last-opened-project-directory!
  ^File [prefs ^File directory]
  (prefs/set-prefs prefs last-opened-project-directory-prefs-key (.getAbsolutePath directory)))

(defn- new-project-location-directory
  ^File [^File last-opened-project-directory]
  (or (when-some [last-opened-project-parent-directory (some-> last-opened-project-directory .getParentFile)]
        (when (fs/existing-directory? last-opened-project-parent-directory)
          last-opened-project-parent-directory))
      (when-some [home-directory (some-> (system/user-home) not-empty io/as-file)]
        (when (fs/existing-directory? home-directory)
          home-directory))))

(def ^:private xform-recent-projects->timestamps-by-path
  (map (fn [{:keys [project-file last-opened]}]
         (assert (instance? Instant last-opened))
         (let [path (.getAbsolutePath ^File project-file)
               timestamp (str last-opened)]
           [path timestamp]))))

(def ^:private xform-timestamps-by-path->recent-projects
  (keep (fn [[path timestamp]]
          (let [file (io/as-file path)
                instant (time/try-parse-instant timestamp)]
            (when (and (some? instant)
                       (<= (.toDays (time/since instant)) 30)
                       (fs/existing-file? file))
              (when-some [title (try-read-project-title file)]
                {:project-file file
                 :last-opened instant
                 :title title}))))))

(def ^:private xform-paths->recent-projects
  (keep (fn [path]
          (let [file (io/as-file path)]
            (when (fs/existing-file? file)
              (when-some [title (try-read-project-title file)]
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

(defn add-recent-project!
  "Updates the recent projects list in the preferences to include a new entry
  for the specified project-file. Uses the current time for the timestamp."
  [prefs ^File project-file]
  (let [recent-projects (recent-projects prefs)
        timestamps-by-path (assoc (into {} xform-recent-projects->timestamps-by-path recent-projects)
                             (.getAbsolutePath project-file) (str (Instant/now)))]
    (prefs/set-prefs prefs recent-projects-prefs-key timestamps-by-path)))

(defn remove-recent-project! [prefs ^File project-file]
  (prefs/update-prefs prefs recent-projects-prefs-key dissoc (.getAbsolutePath project-file)))

;; -----------------------------------------------------------------------------
;; New project creation
;; -----------------------------------------------------------------------------

(defn- download-proj-zip!
  "Downloads a template project zip file. Returns the file or nil if cancelled."
  ^File [url progress-callback cancelled-atom]
  (let [file ^File (fs/create-temp-file! "template-project" ".zip")]
    (net/download! url file :chunk-size 1024 :progress-callback progress-callback :cancelled-derefable cancelled-atom)
    (if @cancelled-atom
      (do
        (fs/delete-file! file)
        nil)
      file)))

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
;; Location field control
;; -----------------------------------------------------------------------------

(defn- location-field-location
  ^File [^Node location-field]
  (ui/with-controls location-field [directory-text title-text]
    (let [directory-path (ui/text directory-text)
          title (ui/text title-text)]
      (when (and (not-empty directory-path)
                 (not-empty title))
        (io/as-file (str directory-path File/separator title))))))

(defn- location-field-title-property
  ^StringProperty [^Node location-field]
  (ui/with-controls location-field [^Label title-text]
    (.textProperty title-text)))

(defn- on-location-field-browse-button-click! [dialog-title ^Event event]
  (let [^Node button (.getSource event)
        location-field (.getParent button)]
    (ui/with-controls location-field [directory-text]
      (let [scene (.getScene button)
            window (.getWindow scene)
            initial-directory (some-> directory-text ui/text not-empty io/as-file)
            initial-directory (when (some-> initial-directory (.exists)) initial-directory)]
        (when-some [directory-path (ui/choose-directory dialog-title initial-directory window)]
          (ui/text! directory-text directory-path))))))

(defn- setup-location-field! [^HBox location-field dialog-title ^File directory]
  (doto location-field
    (ui/add-style! "location-field")
    (ui/children! [(doto (HBox.)
                     (HBox/setHgrow Priority/ALWAYS)
                     (ui/add-style! "path-field")
                     (ui/children! [(doto (Label.)
                                      (HBox/setHgrow Priority/NEVER)
                                      (.setId "directory-text")
                                      (ui/add-styles! ["path-element" "explicit"])
                                      (ui/text! (or (some-> directory .getAbsolutePath) "")))
                                    (doto (Label. File/separator)
                                      (HBox/setHgrow Priority/NEVER)
                                      (ui/add-styles! ["path-element" "implicit"]))
                                    (doto (Label.)
                                      (HBox/setHgrow Priority/NEVER)
                                      (.setMinWidth Region/USE_PREF_SIZE)
                                      (.setId "title-text")
                                      (ui/add-styles! ["path-element" "implicit"]))]))
                   (doto (Button. "\u2022 \u2022 \u2022") ; "* * *" (BULLET)
                     (ui/on-action! (partial on-location-field-browse-button-click! dialog-title)))])))

;; -----------------------------------------------------------------------------
;; Defold logo
;; -----------------------------------------------------------------------------

(def ^:private defold-logo-size-pattern #"-defold-logo-size:\s*(\d+)px\s+(\d+)px")

(defn- try-parse-defold-logo-size [^String style]
  (when-some [width-and-height (not-empty (mapv #(Double/parseDouble %) (drop 1 (re-find defold-logo-size-pattern style))))]
    width-and-height))

(defn- add-defold-logo-svg-paths! [^Node node]
  (doseq [^Parent logo-container (.lookupAll node "Group.defold-logo-container")]
    (when-some [[width height] (try-parse-defold-logo-size (.getStyle logo-container))]
      (ui/children! logo-container (ui/make-defold-logo-paths width height)))))

;; -----------------------------------------------------------------------------
;; Home pane
;; -----------------------------------------------------------------------------

(defn- show-open-from-disk-dialog! [^File last-opened-project-directory close-dialog-and-open-project! ^Event event]
  (let [^Node button (.getSource event)
        scene (.getScene button)
        window (.getWindow scene)
        opts {:title "Open Project"
              :owner-window window
              :directory (some-> last-opened-project-directory .getAbsolutePath)
              :filters [{:description "Defold Project Files"
                         :exts ["*.project"]}]}]
    (when-some [project-file (ui/choose-file opts)]
      (close-dialog-and-open-project! project-file false))))

(defn- timestamp-label [timestamp]
  (doto (Label.)
    (ui/add-style! "timestamp")
    (ui/text! (if (some? timestamp)
                (time/vague-description timestamp)
                "Unknown"))))

(defn- title-label [title matching-indices]
  (doto (fuzzy-choices/make-matched-text-flow title matching-indices)
    (ui/add-style! "title")))

(defn- make-project-entry
  ^Node [^String title ^String description ^Instant timestamp matching-indices on-remove]
  (assert (string? (not-empty title)))
  (assert (or (nil? description) (string? (not-empty description))))
  (doto (HBox.)
    (ui/add-style! "project-entry")
    (ui/children!
      [(doto (VBox.)
         (HBox/setHgrow Priority/ALWAYS)
         (ui/add-style! "title-and-description")
         (ui/children! (if (nil? description)
                         [(title-label title matching-indices)]
                         [(title-label title matching-indices)
                          (doto (Label. description) (ui/add-style! "description"))])))
       (if on-remove
         (doto (VBox.)
           (.setAlignment Pos/CENTER_RIGHT)
           (ui/children!
             [(doto (Button.)
                (ui/add-style! "remove-button")
                (ui/on-action!
                  (fn [_]
                    (on-remove)))
                (.setGraphic (jfx/get-image-view "icons/32/Icons_S_01_SmallClose.png" 10)))
              (timestamp-label timestamp)]))
         (timestamp-label timestamp))])))

(defn- make-recent-project-entry
  ^Node [{:keys [^File project-file ^Instant last-opened ^String title] :as _recent-project}
         recent-projects-list-view
         prefs]
  (let [on-remove #(do
                     (remove-recent-project! prefs project-file)
                     (ui/items! recent-projects-list-view (recent-projects prefs)))]
    (make-project-entry title (.getParent project-file) last-opened nil on-remove)))

(defn- make-home-pane
  ^Parent [last-opened-project-directory show-new-project-pane! close-dialog-and-open-project! prefs]
  (let [home-pane (ui/load-fxml "welcome/home-pane.fxml")
        open-from-disk-buttons (.lookupAll home-pane "#open-from-disk-button")
        show-open-from-disk-dialog! (partial show-open-from-disk-dialog! last-opened-project-directory close-dialog-and-open-project!)]
    (ui/with-controls home-pane [^ListView recent-projects-list ^Node state-empty-recent-projects-list ^Node state-non-empty-recent-projects-list ^ButtonBase open-selected-project-button ^ButtonBase show-new-project-pane-button]
      (let [open-selected-project! (fn []
                                     (when-some [recent-project (first (ui/selection recent-projects-list))]
                                       (close-dialog-and-open-project! (:project-file recent-project) false)))]
        (doseq [open-from-disk-button open-from-disk-buttons]
          (ui/on-action! open-from-disk-button show-open-from-disk-dialog!))
        (ui/on-action! show-new-project-pane-button (fn [_] (show-new-project-pane!)))
        (ui/on-action! open-selected-project-button (fn [_] (open-selected-project!)))
        (b/bind-presence! state-empty-recent-projects-list (b/empty? (.getItems recent-projects-list)))
        (b/bind-presence! state-non-empty-recent-projects-list (b/not (b/empty? (.getItems recent-projects-list))))
        (b/bind-enabled-to-selection! open-selected-project-button recent-projects-list)
        (doto recent-projects-list
          (.setFixedCellSize 56.0)
          (ui/items! (recent-projects prefs))
          (ui/cell-factory!
            (fn [recent-project]
              {:graphic (make-recent-project-entry recent-project recent-projects-list prefs)}))
          (.setOnMouseClicked (ui/event-handler event
                                (when (= 2 (.getClickCount ^MouseEvent event))
                                  (open-selected-project!)))))))
    home-pane))

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

(defn- make-category-button
  ^RadioButton [{:keys [label templates] :as template-project-category}]
  (s/validate TemplateProjectCategory template-project-category)
  (doto (RadioButton.)
    (ui/user-data! :templates templates)
    (ui/text! label)))

(defn- category-button->templates [category-button]
  (ui/user-data category-button :templates))

(defn- new-project-screen-name
  ^String [^ButtonBase selected-template-category-button]
  (str "new-project-"
       (-> (.getText selected-template-category-button)
           (string/replace " " "-")
           (string/lower-case))))

(defn- make-new-project-pane
  ^Parent [new-project-location-directory download-template! welcome-settings ^ToggleGroup category-buttons-toggle-group]
  (doto (ui/load-fxml "welcome/new-project-pane.fxml")
    (ui/with-controls [^ButtonBase create-new-project-button new-project-location-field ^TextField new-project-title-field template-categories ^ListVew template-list]
      (setup-location-field! new-project-location-field "Select New Project Location" new-project-location-directory)
      (b/bind! (location-field-title-property new-project-location-field) (.textProperty new-project-title-field))
      (doto template-list
        (ui/cell-factory! (fn [project-template]
                            {:graphic (make-template-entry project-template)})))

      ;; Configure template category toggle buttons.
      (let [category-buttons (mapv make-category-button (get-in welcome-settings [:new-project :categories]))]

        ;; Add the category buttons to top bar and configure them to toggle between the templates.
        (doseq [^RadioButton category-button category-buttons]
          (.setToggleGroup category-button category-buttons-toggle-group)
          (ui/add-child! template-categories category-button))

        (ui/observe (.selectedToggleProperty category-buttons-toggle-group)
                    (fn [_ old-selected-category-button new-selected-category-button]
                      (when (some? old-selected-category-button)
                        (analytics/track-screen! (new-project-screen-name new-selected-category-button)))
                      (let [templates (category-button->templates new-selected-category-button)]
                        (ui/items! template-list templates)
                        (when (seq templates)
                          (ui/select-index! template-list 0)))))

        ;; Select the first category button.
        (.selectToggle category-buttons-toggle-group (first category-buttons)))

      ;; Update the Title field whenever a template is selected.
      (ui/observe-selection template-list
                            (fn [_ selection]
                              (when-some [selected-project-title (:name (first selection))]
                                (ui/text! new-project-title-field selected-project-title))))

      ;; Configure create-new-project-button.
      (b/bind-enabled-to-selection! create-new-project-button template-list)
      (ui/on-action! create-new-project-button
                     (fn [_]
                       (let [project-template (first (ui/selection template-list))
                             project-title (ui/text new-project-title-field)
                             project-location (location-field-location new-project-location-field)]
                         (cond
                           (string/blank? project-title)
                           (dialogs/make-info-dialog
                             {:title "No Project Title"
                              :icon :icon/triangle-error
                              :header "You must specify a title for the project"})

                           (not= project-title (string/trim project-title))
                           (dialogs/make-info-dialog
                             {:title "Invalid project title"
                              :icon :icon/triangle-error
                              :size :large
                              :header "Whitespace is not allowed around the project title"})

                           (and (.exists project-location)
                                (not (fs/empty-directory? project-location)))
                           (dialogs/make-info-dialog
                             {:title "Conflicting Project Location"
                              :icon :icon/triangle-error
                              :size :large
                              :header "A non-empty folder already exists at the chosen location"})

                           :else
                           (download-template! (:name project-template) (:zip-url project-template) (:skip-root? project-template) project-location project-title))))))))

;; -----------------------------------------------------------------------------
;; Import project pane
;; -----------------------------------------------------------------------------

(defn dashboard-project-name
  ^String [dashboard-project]
  (or (not-empty (:name dashboard-project))
      "Unnamed"))

(def ^:private filter-dashboard-projects (partial fuzzy-choices/filter-options dashboard-project-name dashboard-project-name))

(defn- make-dashboard-project-entry
  ^Node [dashboard-project]
  (let [name (dashboard-project-name dashboard-project)
        description (some-> (:description dashboard-project) string/trim not-empty)
        last-updated (some-> (:last-updated dashboard-project) Instant/ofEpochMilli)
        matching-indices (fuzzy-choices/matching-indices dashboard-project)]
    (make-project-entry name description last-updated matching-indices nil)))

(defn- make-import-project-pane
  ^Parent [new-project-location-directory clone-project! dashboard-client]
  (let [import-project-pane (ui/load-fxml "welcome/import-project-pane.fxml")]
    (ui/with-controls import-project-pane [^ListView dashboard-projects-list empty-dashboard-projects-list-text empty-dashboard-projects-list-overlay ^TextField filter-field filter-field-pane import-project-button import-project-location-field ^TextField import-project-folder-field state-signed-in]
      (let [sign-in-state-property ^SimpleObjectProperty (:sign-in-state-property dashboard-client)
            dashboard-projects-atom (atom nil)
            reload-dashboard-projects! (fn []
                                         (ui/items! dashboard-projects-list [])
                                         (ui/text! empty-dashboard-projects-list-text "Fetching projects...")
                                         (ui/text! filter-field "")
                                         (future
                                           (error-reporting/catch-all!
                                             (with-open [client (client/make-client (:prefs dashboard-client))]
                                               (reset! dashboard-projects-atom (client/fetch-projects client))))))
            refresh-dashboard-projects-list! (fn [filter-text dashboard-projects]
                                               (ui/text! empty-dashboard-projects-list-text
                                                         (if (empty? dashboard-projects)
                                                           "Projects you have synced to the\nDefold cloud will appear here."
                                                           "No matching projects."))
                                               (ui/items! dashboard-projects-list
                                                          (filter-dashboard-projects filter-text dashboard-projects)))]
        ;; Configure sign-in UI elements.
        (login/configure-sign-in-ui-elements! import-project-pane dashboard-client :access-projects)

        ;; Show / hide various elements based on sign-in state.
        (b/bind-presence! state-signed-in (b/= :signed-in sign-in-state-property))
        (b/bind-presence! filter-field-pane (b/= :signed-in sign-in-state-property))
        (b/bind-presence! empty-dashboard-projects-list-overlay (b/empty? (.getItems dashboard-projects-list)))

        ;; Configure location field.
        (setup-location-field! import-project-location-field "Select Import Location" new-project-location-directory)
        (b/bind! (location-field-title-property import-project-location-field) (.textProperty import-project-folder-field))

        ;; Configure Import Project button.
        (doto import-project-button
          (b/bind-enabled-to-selection! dashboard-projects-list)
          (ui/on-action! (fn [_]
                           (let [dashboard-project (first (ui/selection dashboard-projects-list))
                                 project-title (:name dashboard-project)
                                 project-folder (ui/text import-project-folder-field)
                                 clone-directory (location-field-location import-project-location-field)]
                             (cond
                               (string/blank? project-folder)
                               (dialogs/make-info-dialog
                                 {:title "No Destination Folder"
                                  :icon :icon/triangle-error
                                  :size :large
                                  :header "You must specify a destination folder for the project"})

                               (not= project-folder (string/trim project-folder))
                               (dialogs/make-info-dialog
                                 {:title "Invalid Destination Folder"
                                  :icon :icon/triangle-error
                                  :size :large
                                  :header "Whitespace is not allowed around the folder name"})

                               (and (.exists clone-directory)
                                    (not (fs/empty-directory? clone-directory)))
                               (dialogs/make-info-dialog
                                 {:title "Conflicting Import Location"
                                  :icon :icon/triangle-error
                                  :size :large
                                  :header "A non-empty folder already exists at the chosen location"})

                               :else
                               (clone-project! project-title (:repository-url dashboard-project) clone-directory))))))

        ;; Configure the projects list.
        (doto dashboard-projects-list
          (.setFixedCellSize 56.0)
          (ui/cell-factory! (fn [dashboard-project]
                              {:graphic (make-dashboard-project-entry dashboard-project)})))

        ;; Update the Title field whenever a project is selected.
        (ui/observe-selection dashboard-projects-list
                              (fn [_ selection]
                                (when-some [selected-project-title (:name (first selection))]
                                  (ui/text! import-project-folder-field selected-project-title))))

        ;; Refresh the projects list when the list of dashboard projects or the filter field changes.
        (add-watch dashboard-projects-atom
                   ::dashboard-projects-watch
                   (fn [_ _ _ dashboard-projects]
                     (ui/run-later
                       (refresh-dashboard-projects-list! (ui/text filter-field) dashboard-projects))))

        (ui/observe (.textProperty filter-field)
                    (fn [_ _ filter-text]
                      (refresh-dashboard-projects-list! filter-text @dashboard-projects-atom)))

        ;; Refresh the projects list whenever we sign-in.
        (ui/observe sign-in-state-property (fn [_ _ sign-in-state]
                                             (when (= :signed-in sign-in-state) ; I.e. became :signed-in.
                                               (reload-dashboard-projects!))))

        (when (= :signed-in (.get sign-in-state-property))
          (reload-dashboard-projects!))))
    import-project-pane))

;; -----------------------------------------------------------------------------
;; Automatic updates
;; -----------------------------------------------------------------------------

(defn- init-pending-update-indicator! [stage update-link progress-bar progress-vbox updater]
  (let [render-progress! (progress/throttle-render-progress
                           (fn [progress]
                             (ui/run-later
                               (ui/visible! progress-vbox (progress/done? progress))
                               (ui/visible! progress-bar (not (progress/done? progress)))
                               (ui/render-progress-bar! progress progress-bar))))
        install-and-restart! #(ui.updater/install-and-restart! stage updater)]
    (ui.updater/init! stage update-link updater install-and-restart! render-progress!)))

;; -----------------------------------------------------------------------------
;; Welcome dialog
;; -----------------------------------------------------------------------------

(defn- make-pane-button [label pane]
  (doto (RadioButton.)
    (.setFocusTraversable false)
    (ui/user-data! :pane pane)
    (ui/add-style! "pane-button")
    (ui/text! label)))

(defn- pane-button->pane
  ^Parent [pane-button]
  (ui/user-data pane-button :pane))

(defn- show-progress!
  ^ProgressBar [^Parent root ^String header-text ^String cancel-button-text cancel!]
  (ui/with-controls root [^ProgressBar progress-bar ^ButtonBase progress-cancel-button progress-header ^Parent progress-overlay]
    (ui/text! progress-header header-text)
    (ui/text! progress-cancel-button cancel-button-text)
    (ui/enable! progress-cancel-button true)
    (.setProgress progress-bar ProgressBar/INDETERMINATE_PROGRESS)
    (.setManaged progress-overlay true)
    (.setVisible progress-overlay true)
    (.setOnMouseClicked progress-cancel-button
                        (ui/event-handler event
                          (.setOnMouseClicked progress-cancel-button nil)
                          (ui/enable! progress-cancel-button false)
                          (cancel!)))
    progress-bar))

(defn- hide-progress! [^Parent root]
  (ui/with-controls root [^Parent progress-overlay]
    (.setManaged progress-overlay false)
    (.setVisible progress-overlay false)))

(defn show-welcome-dialog!
  ([prefs dashboard-client updater open-project-fn]
   (show-welcome-dialog! prefs dashboard-client updater open-project-fn nil))
  ([prefs dashboard-client updater open-project-fn opts]
   (let [[welcome-settings
          welcome-settings-load-error] (try
                                         [(load-welcome-settings "welcome/welcome.edn") nil]
                                         (catch Exception error
                                           [nil error]))
         root (ui/load-fxml "welcome/welcome-dialog.fxml")
         stage (ui/make-dialog-stage)
         scene (Scene. root)
         last-opened-project-directory (last-opened-project-directory prefs)
         new-project-location-directory (new-project-location-directory last-opened-project-directory)
         close-dialog-and-open-project! (fn [^File project-file newly-created?]
                                          (when (fs/existing-file? project-file)
                                            (set-last-opened-project-directory! prefs (.getParentFile project-file))
                                            (ui/close! stage)
                                            (analytics/track-event! "welcome" "open-project")
                                            ;; NOTE: Old comment copied from old open-welcome function in boot.clj
                                            ;; We load the project in the same class-loader as welcome is loaded from.
                                            ;; In other words, we can't reuse the welcome page and it has to be closed.
                                            ;; We should potentially changed this when we have uberjar support and hence
                                            ;; faster loading.
                                            (open-project-fn (.getAbsolutePath project-file) newly-created?)
                                            nil))
         clone-project! (fn [project-title repository-url dest-directory]
                          (let [cancelled-atom (atom false)
                                progress-bar (show-progress! root (str "Downloading " project-title) "Cancel Download" #(reset! cancelled-atom true))
                                progress-monitor (git/make-clone-monitor progress-bar cancelled-atom)
                                credentials (git/credentials prefs)]
                            (analytics/track-event! "welcome" "clone-project")
                            (future
                              (try
                                (git/clone! credentials repository-url dest-directory progress-monitor)
                                (ui/run-later
                                  (if @cancelled-atom
                                    (do
                                      (hide-progress! root)
                                      (fs/delete-directory! dest-directory {:fail :silently}))
                                    (close-dialog-and-open-project! (io/file dest-directory "game.project") false)))
                                (catch Throwable error
                                  (fs/delete-directory! dest-directory {:fail :silently})
                                  (ui/run-later
                                    (hide-progress! root)
                                    (error-reporting/report-exception! error)))))))
         download-template! (fn [template-title zip-url skip-root? dest-directory project-title]
                              (let [cancelled-atom (atom false)
                                    progress-bar (show-progress! root (str "Downloading " template-title) "Cancel Download" #(reset! cancelled-atom true))
                                    progress-callback (fn [^long done ^long total]
                                                        (when (pos? total)
                                                          (let [progress (/ (double done) (double total))]
                                                            (ui/run-later
                                                              (.setProgress progress-bar progress)))))]
                                (analytics/track-event! "welcome" "download-template" template-title)
                                (future
                                  (try
                                    (if-some [template-zip-file (download-proj-zip! zip-url progress-callback cancelled-atom)]
                                      (let [project-file (io/file dest-directory "game.project")]
                                        (expand-proj-zip! template-zip-file dest-directory skip-root?)
                                        (write-project-title! project-file project-title)
                                        (ui/run-later
                                          (if @cancelled-atom
                                            (do
                                              (hide-progress! root)
                                              (fs/delete-directory! dest-directory {:fail :silently}))
                                            (close-dialog-and-open-project! project-file true))))
                                      (ui/run-later
                                        (hide-progress! root)))
                                    (catch Throwable error
                                      (fs/delete-directory! dest-directory {:fail :silently})
                                      (ui/run-later
                                        (hide-progress! root)
                                        (cond
                                          (instance? UnknownHostException error)
                                          (dialogs/make-info-dialog
                                            {:title "No Internet Connection"
                                             :icon :icon/triangle-error
                                             :header "You must be connected to the internet to download project content"})

                                          (instance? SocketException error)
                                          (dialogs/make-info-dialog
                                            {:title "Host Unreachable"
                                             :icon :icon/triangle-error
                                             :header "A firewall might be blocking network connections"
                                             :content (.getMessage error)})

                                          (instance? SocketTimeoutException error)
                                          (dialogs/make-info-dialog
                                            {:title "Host Not Responding"
                                             :icon :icon/triangle-error
                                             :header "The connection timed out"
                                             :content (.getMessage error)})

                                          (instance? SSLException error)
                                          (dialogs/make-info-dialog
                                            {:title "SSL Connection Error"
                                             :icon :icon/triangle-error
                                             :header "Could not establish an SSL connection"
                                             :content {:fx/type :text-flow
                                                       :style-class "dialog-content-padding"
                                                       :children [{:fx/type :text
                                                                   :text (str "Common causes are:\n"
                                                                              "\u00A0\u00A0\u2022\u00A0 Antivirus software configured to scan encrypted connections\n"
                                                                              "\u00A0\u00A0\u2022\u00A0 Expired or misconfigured server certificate\n"
                                                                              "\u00A0\u00A0\u2022\u00A0 Untrusted server certificate\n"
                                                                              "\n"
                                                                              "The following FAQ may apply: ")}
                                                                  {:fx/type :hyperlink
                                                                   :text "PKIX path building failed"
                                                                   :on-action (fn [_]
                                                                                (ui/open-url "https://github.com/defold/editor2-issues/blob/master/faq/pkixpathbuilding.md"))}
                                                                  {:fx/type :text
                                                                   :text (str "\n\n" (string/replace (.getMessage error) ": " ":\n\u00A0\u00A0"))}]}})

                                          :else
                                          (error-reporting/report-exception! error))))))))
         left-pane (.lookup root "#left-pane")
         pane-buttons-container (.lookup left-pane "#pane-buttons-container")
         sign-out-button (.lookup left-pane "#sign-out-button")
         pane-buttons-toggle-group (ToggleGroup.)
         template-category-buttons-toggle-group (ToggleGroup.)
         new-project-pane-button (make-pane-button "NEW PROJECT" (make-new-project-pane new-project-location-directory download-template! welcome-settings template-category-buttons-toggle-group))
         show-new-project-pane! (fn [] (.selectToggle pane-buttons-toggle-group new-project-pane-button))
         home-pane-button (make-pane-button "HOME"
                                            (make-home-pane last-opened-project-directory
                                                            show-new-project-pane!
                                                            close-dialog-and-open-project!
                                                            prefs))
         import-project-button (make-pane-button "IMPORT PROJECT" (make-import-project-pane new-project-location-directory clone-project! dashboard-client))
         pane-buttons [home-pane-button new-project-pane-button import-project-button]
         screen-name (fn [selected-pane-button selected-template-category-button]
                       (condp = selected-pane-button
                         home-pane-button "home"
                         import-project-button "import-project"
                         new-project-pane-button (new-project-screen-name selected-template-category-button)))]

     ;; Add Defold logo SVG paths.
     (doseq [pane (map pane-button->pane pane-buttons)]
       (add-defold-logo-svg-paths! pane))

     ;; Make ourselves the main stage.
     (ui/set-main-stage stage)

     ;; If a sign-in was initiated but never completed, make sure we start from
     ;; the beginning on successive sign-in attempts.
     (ui/on-closed! stage (fn [_] (login/abort-incomplete-sign-in! dashboard-client)))

     ;; Install pending update check.
     (when (some? updater)
       (ui/with-controls left-pane [update-link update-progress-bar update-progress-vbox]
         (init-pending-update-indicator! stage
                                         update-link
                                         update-progress-bar
                                         update-progress-vbox
                                         updater)))

     ;; Add the pane buttons to the left panel and configure them to toggle between the panes.
     (doseq [^RadioButton pane-button pane-buttons]
       (.setToggleGroup pane-button pane-buttons-toggle-group)
       (ui/add-child! pane-buttons-container pane-button))

     (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                 (fn [_ old-selected-pane-button new-selected-pane-button]
                   (when (some? old-selected-pane-button)
                     (analytics/track-screen! (screen-name new-selected-pane-button (.getSelectedToggle template-category-buttons-toggle-group))))
                   (let [pane (pane-button->pane new-selected-pane-button)]
                     (ui/with-controls root [dialog-contents]
                       (ui/children! dialog-contents [left-pane pane])))))

     ;; Select the home pane button.
     (.selectToggle pane-buttons-toggle-group home-pane-button)

     ;; Configure the sign-out button.
     (login/configure-sign-out-button! sign-out-button dashboard-client)

     ;; Apply opts if supplied.
     (when-some [{:keys [x y pane-index]} opts]
       (.setX stage x)
       (.setY stage y)
       (.selectToggle pane-buttons-toggle-group (nth pane-buttons pane-index)))

     ;; Press Cmd-R to reload the view in dev mode.
     (when (system/defold-dev?)
       (.setOnKeyPressed root (ui/event-handler event
                                (let [key-event ^KeyEvent event
                                      selected-pane-button (.getSelectedToggle pane-buttons-toggle-group)]
                                  (when (and (.isShortcutDown key-event)
                                             (= "r" (.getText key-event)))
                                    (ui/close! stage)
                                    (show-welcome-dialog! prefs dashboard-client updater open-project-fn
                                                          {:x (.getX stage)
                                                           :y (.getY stage)
                                                           :pane-index (first (keep-indexed (fn [pane-index pane-button]
                                                                                              (when (= selected-pane-button pane-button)
                                                                                                pane-index))
                                                                                            pane-buttons))}))))))

     ;; Report invalid welcome settings.
     (when (some? welcome-settings-load-error)
       (error-reporting/report-exception! welcome-settings-load-error)

       ;; If this happens in dev mode, it is likely due to an invalid welcome.edn file.
       ;; Display the error message on the new project pane, so we can fix and reload.
       (when (system/defold-dev?)
         (let [new-project-pane-button (second pane-buttons)
               new-project-pane (pane-button->pane new-project-pane-button)]
           (.selectToggle pane-buttons-toggle-group new-project-pane-button)
           (ui/children! new-project-pane [(doto (TextArea.)
                                             (.setPrefHeight 10000.0)
                                             (.setEditable false)
                                             (.setStyle "-fx-font-family: 'Dejavu Sans Mono'; -fx-font-size: 12px;")
                                             (ui/text! (if (and (instance? ExceptionInfo welcome-settings-load-error)
                                                                (= :schema.core/error (:type (ex-data welcome-settings-load-error))))
                                                         (let [ex-data (ex-data welcome-settings-load-error)]
                                                           (str "Schema Validation failed when loading `welcome.edn`.\n\n"
                                                                "Use Command+R to reload once you've fixed the error.\n\n"
                                                                "Errors were found here:\n"
                                                                (with-out-str (pprint/pprint (:error ex-data)))
                                                                "\n"
                                                                "When matched against this schema:\n"
                                                                (with-out-str (pprint/pprint (:schema ex-data)))))
                                                         (with-out-str (pprint/pprint welcome-settings-load-error))))
                                             (.positionCaret 0))]))))

     ;; Show the dialog.
     (analytics/track-event! "welcome" "show-welcome")
     (analytics/track-screen! (screen-name (.getSelectedToggle pane-buttons-toggle-group) (.getSelectedToggle template-category-buttons-toggle-group)))
     (.setScene stage scene)
     (ui/show! stage))))
