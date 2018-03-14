(ns editor.welcome
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.string :as string]
            [editor.client :as client]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.import :as import]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [schema.core :as s]
            [util.net :as net]
            [util.time :as time]
            [editor.jfx :as jfx])
  (:import (clojure.lang ExceptionInfo)
           (java.io File FileOutputStream PushbackReader)
           (java.net MalformedURLException URL URLEncoder)
           (java.security MessageDigest)
           (java.time Instant)
           (java.util.zip ZipInputStream)
           (javafx.animation AnimationTimer)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleBooleanProperty)
           (javafx.beans.value ObservableNumberValue)
           (javafx.event Event)
           (javafx.scene Node Scene Parent)
           (javafx.scene.control ButtonBase Label ListView RadioButton TextArea ToggleGroup)
           (javafx.scene.image ImageView Image)
           (javafx.scene.input KeyEvent MouseEvent)
           (javafx.scene.layout HBox Priority StackPane VBox)
           (javafx.scene.shape Rectangle)
           (javafx.scene.text Text TextFlow)
           (javafx.stage Stage)
           (org.apache.commons.io FilenameUtils)))

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
;; Preferences management
;; -----------------------------------------------------------------------------

(defn- open-project-directory
  ^File [prefs]
  (when-some [directory (io/as-file (prefs/get-prefs prefs open-project-directory-prefs-key nil))]
    (when (fs/existing-directory? directory)
      directory)))

(defn- set-open-project-directory!
  ^File [prefs ^File directory]
  (prefs/set-prefs prefs open-project-directory-prefs-key (.getAbsolutePath directory)))

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

(defn- add-recent-project!
  "Updates the recent projects list in the preferences to include a new entry
  for the specified project-file. Uses the current time for the timestamp."
  [prefs recent-projects ^File project-file]
  (let [timestamps-by-path (assoc (into {} xform-recent-projects->timestamps-by-path recent-projects)
                             (.getAbsolutePath project-file) (str (Instant/now)))]
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
;; Dashboard client
;; -----------------------------------------------------------------------------

(defn- make-dashboard-client [prefs]
  (let [client (client/make-client prefs)
        logged-in-property (SimpleBooleanProperty. (login/logged-in? prefs client))]
    {:logged-in-property logged-in-property
     :prefs prefs}))

(defn- login!
  "Log in to access Defold dashboard. If the user is already logged in, returns
  true. Else display a login prompt and block until the user has either logged
  in or cancelled, then return true if the login completed successfully."
  [{:keys [^SimpleBooleanProperty logged-in-property prefs] :as _dashboard-client}]
  (let [logged-in? (login/login prefs)]
    (.set logged-in-property logged-in?)
    logged-in?))

(defn- logout!
  "Log out the active user from the Defold dashboard."
  [{:keys [^SimpleBooleanProperty logged-in-property prefs] :as _dashboard-client}]
  (login/logout prefs)
  (.set logged-in-property false))

(defn- fetch-dashboard-projects [{:keys [prefs] :as _dashboard-client}]
  (import/fetch-projects prefs))

;; -----------------------------------------------------------------------------
;; Gravatar images
;; -----------------------------------------------------------------------------

(defn- md5-hash
  ^String [^String string]
  (.toString (BigInteger. 1 (.digest (MessageDigest/getInstance "MD5") (.getBytes string "UTF-8"))) 16))

(def ^:private gravatar-id (comp md5-hash string/lower-case string/trim))

(defn- gravatar-image-url
  ^String [^long pixel-size ^String email-address]
  (assert (<= 1 pixel-size 2048))
  (format "https://secure.gravatar.com/avatar/%s.jpg?s=%d&d=mm"
          (gravatar-id email-address)
          pixel-size))

(def ^:private gravatar-image
  (memoize
    (fn gravatar-image [^double point-size ^String email-address]
      (Image. (gravatar-image-url (long (* 2 point-size)) email-address) true))))

(defn- gravatar-image-view
  ^ImageView [^double point-size ^String email-address]
  (doto (ImageView.)
    (ui/add-style! "gravatar")
    (.setImage (gravatar-image point-size email-address))
    (.setFitWidth point-size)
    (.setFitHeight point-size)))

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

(defn- setup-directory-browse-field! [^Parent browse-field dialog-title]
  (let [button (.lookup browse-field "Button")]
    (ui/on-action! button (partial on-directory-field-browse-button-click! dialog-title))))

(defn- browse-field-file
  ^File [^Node browse-field]
  (when-some [path (not-empty (ui/text (.lookup browse-field "TextField")))]
    (io/as-file path)))

(defn- set-browse-field-file! [^Node browse-field ^File file]
  (ui/text! (.lookup browse-field "TextField")
            (or (some-> file .getAbsolutePath) "")))

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

(defn- show-open-from-disk-dialog! [^File open-project-directory close-dialog-and-open-project! ^Event event]
  (let [^Node button (.getSource event)
        scene (.getScene button)
        window (.getWindow scene)
        opts {:title "Open Project"
              :owner-window window
              :directory (some-> open-project-directory .getAbsolutePath)
              :filters [{:description "Defold Project Files"
                         :exts ["*.project"]}]}]
    (when-some [project-file (ui/choose-file opts)]
      (close-dialog-and-open-project! project-file))))

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
                                    (Label. (time/vague-description last-opened))]))])))

(defn- make-home-pane
  ^Parent [open-project-directory close-dialog-and-open-project! recent-projects]
  (doto (ui/load-fxml "welcome/home-pane.fxml")
    (ui/with-controls [^ListView recent-projects-list ^Node empty-recent-projects-list-overlay open-from-disk-button]
      (ui/on-action! open-from-disk-button (partial show-open-from-disk-dialog! open-project-directory close-dialog-and-open-project!))
      (ui/bind-presence! empty-recent-projects-list-overlay (Bindings/isEmpty (.getItems recent-projects-list)))
      (doto recent-projects-list
        (ui/items! recent-projects)
        (ui/cell-factory! (fn [recent-project]
                            {:graphic (make-recent-project-entry recent-project)}))
        (.setOnMouseClicked (ui/event-handler event
                              (when (= 2 (.getClickCount ^MouseEvent event))
                                (when-some [recent-project (first (ui/selection recent-projects-list))]
                                  (close-dialog-and-open-project! (:project-file recent-project))))))))))

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

(defn- make-new-project-pane
  ^Parent [open-project-directory close-dialog-and-open-project! welcome-settings]
  (doto (ui/load-fxml "welcome/new-project-pane.fxml")
    (ui/with-controls [template-categories ^ListView template-list location-browse-field ^ButtonBase submit-button]
      (doto location-browse-field
        (setup-directory-browse-field! "Select New Project Location")
        (set-browse-field-file! open-project-directory))
      (doto template-list
        (ui/cell-factory! (fn [project-template]
                            {:graphic (make-template-entry project-template)})))

      ;; Configure template category toggle buttons.
      (let [category-buttons-toggle-group (ToggleGroup.)
            category-buttons (mapv make-category-button (get-in welcome-settings [:new-project :categories]))]

        ;; Add the category buttons to top bar and configure them to toggle between the templates.
        (doseq [^RadioButton category-button category-buttons]
          (.setToggleGroup category-button category-buttons-toggle-group)
          (ui/add-child! template-categories category-button))

        (ui/observe (.selectedToggleProperty category-buttons-toggle-group)
                    (fn [_ _ selected-category-button]
                      (let [templates (category-button->templates selected-category-button)]
                        (ui/items! template-list templates))))

        ;; Select the first category button.
        (.selectToggle category-buttons-toggle-group (first category-buttons)))

      ;; Disable submit-button when no template is selected.
      (.bind (.disableProperty submit-button)
             (Bindings/equal -1 ^ObservableNumberValue (.selectedIndexProperty (.getSelectionModel template-list))))

      (ui/on-action! submit-button
                     (fn [_]
                       (when-some [project-template (first (ui/selection template-list))]
                         (let [project-location (browse-field-file location-browse-field)]
                           (if (fs/empty-directory? project-location)
                             (when-some [template-zip-file (download-proj-zip! (:zip-url project-template))]
                               (expand-proj-zip! template-zip-file project-location (:skip-root? project-template))
                               (close-dialog-and-open-project! (io/file project-location "game.project")))
                             (dialogs/make-message-box "Invalid Project Location"
                                                       "Please select an empty directory for the new project.")))))))))

;; -----------------------------------------------------------------------------
;; Import project pane
;; -----------------------------------------------------------------------------

(defn- refresh-dashboard-projects-list! [^ListView dashboard-projects-list dashboard-client]
  (ui/items! dashboard-projects-list (fetch-dashboard-projects dashboard-client)))

#_{:description "Project Description",
   :last-updated 1396602464192,
   :tracking-id "EBEBEBEBEBEBEBEB",
   :repository-url "http://cr.defold.se:9998/prjs/8888",
   :name "Project Name",
   :created 1476041761199,
   :id 88888,
   :members [{:id 88888,
              :email "first.last@domain.com",
              :first-name "First",
              :last-name "Last"}],
   :i-os-executable-url "https://cr.defold.se/projects/88888/8888/engine_manifest/ios/<>",
   :owner {:id 88888,
           :email "first.last@domain.com",
           :first-name "First",
           :last-name "Last"}}

(defn- make-dashboard-project-entry
  ^Node [dashboard-project]
  (let [name (or (not-empty (:name dashboard-project)) "Unnamed")
        description (or (not-empty (:description dashboard-project)) "No description")
        last-updated (Instant/ofEpochMilli (:last-updated dashboard-project))]
    (doto (VBox.)
      (ui/add-style! "dashboard-project-entry")
      (ui/children! [(doto (HBox.)
                       (ui/children! (into []
                                           (comp (map :email)
                                                 (map (partial gravatar-image-view 32.0)))
                                           (:members dashboard-project))))
                     (doto (Text. name)
                       (ui/add-style! "title"))
                     (doto (HBox.)
                       (ui/children! [(doto (Label. description)
                                        (HBox/setHgrow Priority/ALWAYS))
                                      (Label. (time/vague-description last-updated))]))]))))

(defn- make-import-project-pane
  ^Parent [open-project-directory close-dialog-and-open-project! dashboard-client]
  (let [^SimpleBooleanProperty logged-in-property (:logged-in-property dashboard-client)]
    (doto (ui/load-fxml "welcome/import-project-pane.fxml")
      (ui/with-controls [^ButtonBase create-account-button ^ListView dashboard-projects-list ^Node empty-dashboard-projects-list-overlay open-from-disk-button ^ButtonBase sign-in-button ^Node state-signed-in ^Node state-not-signed-in]
        (ui/bind-presence! state-signed-in logged-in-property)
        (ui/bind-presence! state-not-signed-in (Bindings/not logged-in-property))
        (ui/bind-presence! empty-dashboard-projects-list-overlay (Bindings/isEmpty (.getItems dashboard-projects-list)))
        (ui/on-action! open-from-disk-button (partial show-open-from-disk-dialog! open-project-directory close-dialog-and-open-project!))
        (ui/on-action! sign-in-button (fn [_] (login! dashboard-client)))
        (ui/on-action! create-account-button (fn [_] (ui/open-url "https://www.defold.com")))
        (ui/cell-factory! dashboard-projects-list (fn [dashboard-project]
                                                    {:graphic (make-dashboard-project-entry dashboard-project)}))
        (ui/observe logged-in-property (fn [_ _ became-logged-in?]
                                         (when became-logged-in?
                                           (refresh-dashboard-projects-list! dashboard-projects-list dashboard-client))))
        (when (.get logged-in-property)
          (refresh-dashboard-projects-list! dashboard-projects-list dashboard-client))))))

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

(defn show-welcome-dialog!
  ([prefs update-context open-project-fn]
   (show-welcome-dialog! prefs update-context open-project-fn nil))
  ([prefs update-context open-project-fn opts]
   (let [[welcome-settings
          welcome-settings-load-error] (try
                                         [(load-welcome-settings "welcome/welcome.edn") nil]
                                         (catch Exception error
                                           [nil error]))
         root (ui/load-fxml "welcome/welcome-dialog.fxml")
         stage (ui/make-dialog-stage)
         scene (Scene. root)
         recent-projects (recent-projects prefs)
         open-project-directory (open-project-directory prefs)
         close-dialog-and-open-project! (fn [^File project-file]
                                          (when (fs/existing-file? project-file)
                                            (set-open-project-directory! prefs (.getParentFile project-file))
                                            (add-recent-project! prefs recent-projects project-file)
                                            (ui/close! stage)

                                            ;; NOTE (TODO): We load the project in the same class-loader as welcome is loaded from.
                                            ;; In other words, we can't reuse the welcome page and it has to be closed.
                                            ;; We should potentially changed this when we have uberjar support and hence
                                            ;; faster loading.
                                            (open-project-fn (.getAbsolutePath project-file))
                                            nil))
         dashboard-client (make-dashboard-client prefs)
         left-pane ^Parent (.lookup root "#left-pane")
         pane-buttons-toggle-group (ToggleGroup.)
         pane-buttons [(make-pane-button "HOME" (make-home-pane open-project-directory close-dialog-and-open-project! recent-projects))
                       (make-pane-button "NEW PROJECT" (make-new-project-pane open-project-directory close-dialog-and-open-project! welcome-settings))
                       (make-pane-button "IMPORT PROJECT" (make-import-project-pane open-project-directory close-dialog-and-open-project! dashboard-client))]]

     ;; Add Defold logo SVG paths.
     (doseq [pane (map pane-button->pane pane-buttons)]
       (add-defold-logo-svg-paths! pane))

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
                                    (show-welcome-dialog! prefs update-context open-project-fn
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
     (.setScene stage scene)
     (ui/show! stage))))
