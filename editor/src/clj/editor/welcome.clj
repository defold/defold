(ns editor.welcome
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.string :as string]
            [editor.client :as client]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.git :as git]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.updater :as updater]
            [schema.core :as s]
            [util.net :as net]
            [util.thread-util :refer [preset!]]
            [util.time :as time])
  (:import (clojure.lang ExceptionInfo)
           (com.dynamo.cr.protocol.proto Protocol$ProjectInfoList)
           (java.io File FileOutputStream PushbackReader)
           (java.net MalformedURLException SocketException SocketTimeoutException URL UnknownHostException)
           (java.time Instant)
           (java.util.zip ZipInputStream)
           (javafx.animation AnimationTimer)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleObjectProperty StringProperty)
           (javafx.event Event)
           (javafx.scene Node Parent Scene)
           (javafx.scene.control Button ButtonBase Label ListView ProgressBar RadioButton TextArea TextField ToggleGroup)
           (javafx.scene.image ImageView Image)
           (javafx.scene.input Clipboard ClipboardContent KeyEvent MouseEvent)
           (javafx.scene.layout HBox Priority Region StackPane VBox)
           (javafx.scene.shape Rectangle)
           (javafx.scene.text Text TextFlow)
           (javafx.stage Stage WindowEvent)
           (org.apache.commons.io FilenameUtils)))

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

;; -----------------------------------------------------------------------------
;; New project creation
;; -----------------------------------------------------------------------------

(defn- download-proj-zip!
  "Downloads a template project zip file. Returns the file or nil if cancelled."
  ^File [url progress-callback cancelled-atom]
  (let [file ^File (fs/create-temp-file! "template-project" ".zip")]
    (with-open [output (FileOutputStream. file)]
      ;; The long timeout suggests we need some progress reporting even before d/l starts.
      (let [timeout 2000]
        (net/download! url output :connect-timeout timeout :read-timeout timeout :chunk-size 1024 :progress-callback progress-callback :cancelled-atom cancelled-atom)))
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
;; Dashboard client
;; -----------------------------------------------------------------------------

(defn- sign-in-state [^SimpleObjectProperty sign-in-state-property]
  (.get sign-in-state-property))

(defn- set-sign-in-state! [^SimpleObjectProperty sign-in-state-property sign-in-state]
  (assert (case sign-in-state (:not-signed-in :browser-open :signed-in) true false))
  (.set sign-in-state-property sign-in-state))

(defn- make-dashboard-client [prefs]
  (let [client (client/make-client prefs)
        sign-in-response-server-atom (atom nil)
        sign-in-state-property (doto (SimpleObjectProperty.)
                                 (set-sign-in-state! (if (login/logged-in? prefs client)
                                                       :signed-in
                                                       :not-signed-in)))]
    {:sign-in-response-server-atom sign-in-response-server-atom
     :sign-in-state-property sign-in-state-property
     :prefs prefs}))

(defn- shutdown-dashboard-client! [{:keys [sign-in-response-server-atom] :as _dashboard-client}]
  (when-some [sign-in-response-server (preset! sign-in-response-server-atom nil)]
    (login/stop-server-now! sign-in-response-server)))

(defn- restart-server! [old-sign-in-response-server client server-opts]
  (when (some? old-sign-in-response-server)
    (login/stop-server-now! old-sign-in-response-server))
  (login/make-server client server-opts))

(defn- begin-sign-in!
  "If the user is already signed in, does nothing. Otherwise, open a sign-in
  page in the system-configured web browser. Start a server that awaits the
  oauth response redirect from the browser."
  [{:keys [prefs sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (assert (= :not-signed-in (sign-in-state sign-in-state-property)))
  (let [client (client/make-client prefs)]
    (if (login/logged-in? prefs client)
      (set-sign-in-state! sign-in-state-property :signed-in)
      (let [server-opts {:on-success (fn [exchange-info]
                                       (ui/run-later
                                         (login/stop-server-soon! (preset! sign-in-response-server-atom nil))
                                         (login/set-prefs-from-successful-login! prefs exchange-info)
                                         (set-sign-in-state! sign-in-state-property :signed-in)))
                         :on-error (fn [exception]
                                     (ui/run-later
                                       (login/stop-server-soon! (preset! sign-in-response-server-atom nil))
                                       (set-sign-in-state! sign-in-state-property :not-signed-in)
                                       (error-reporting/report-exception! exception)))}
            sign-in-response-server (swap! sign-in-response-server-atom restart-server! client server-opts)
            sign-in-page-url (login/login-page-url sign-in-response-server)]
        (set-sign-in-state! sign-in-state-property :browser-open)
        (ui/open-url sign-in-page-url)))))

(defn- cancel-sign-in!
  "Cancel the sign-in process and return to the :not-signed-in screen."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (assert (= :browser-open (sign-in-state sign-in-state-property)))
  (login/stop-server-now! (preset! sign-in-response-server-atom nil))
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn- copy-sign-in-url!
  "Copy the sign-in url from the sign-in response server to the clipboard.
  If the sign-in response server is not running, does nothing."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client} ^Clipboard clipboard]
  (assert (= :browser-open (sign-in-state sign-in-state-property)))
  (when-some [sign-in-response-server @sign-in-response-server-atom]
    (let [sign-in-page-url (login/login-page-url sign-in-response-server)]
      (.setContent clipboard (doto (ClipboardContent.)
                               (.putString sign-in-page-url)
                               (.putUrl sign-in-page-url))))))

(defn- sign-out!
  "Sign out the active user from the Defold dashboard."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client}]
  (assert (= :signed-in (sign-in-state sign-in-state-property)))
  (login/logout prefs)
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn- compare-project-names [p1 p2]
  (.compareToIgnoreCase ^String (:name p1) ^String (:name p2)))

(defn- fetch-dashboard-projects [{:keys [prefs] :as _dashboard-client}]
  (let [client (client/make-client prefs)]
    (let [project-info-list (client/rget client "/projects/-1" Protocol$ProjectInfoList)]
      (sort compare-project-names (:projects project-info-list)))))

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

(defn- make-project-entry
  ^Node [^String title ^String description ^Instant timestamp matching-indices]
  (assert (string? (not-empty title)))
  (assert (or (nil? description) (string? (not-empty description))))
  (doto (HBox.)
    (ui/add-style! "project-entry")
    (ui/children! [(doto (VBox.)
                     (HBox/setHgrow Priority/ALWAYS)
                     (ui/add-style! "title-and-description")
                     (ui/children! (if (nil? description)
                                     [(doto (fuzzy-choices/make-matched-text-flow title matching-indices) (ui/add-style! "title"))]
                                     [(doto (fuzzy-choices/make-matched-text-flow title matching-indices) (ui/add-style! "title"))
                                      (doto (Label. description) (ui/add-style! "description"))])))
                   (doto (Label.)
                     (ui/add-style! "timestamp")
                     (ui/text! (if (some? timestamp)
                                 (time/vague-description timestamp)
                                 "Unknown")))])))

(defn- make-recent-project-entry
  ^Node [{:keys [^File project-file ^Instant last-opened ^String title] :as _recent-project}]
  (make-project-entry title (.getParent project-file) last-opened nil))

(defn- make-home-pane
  ^Parent [last-opened-project-directory close-dialog-and-open-project! recent-projects]
  (doto (ui/load-fxml "welcome/home-pane.fxml")
    (ui/with-controls [^ListView recent-projects-list ^Node empty-recent-projects-list-overlay ^ButtonBase open-from-disk-button ^ButtonBase open-selected-project-button]
      (let [open-selected-project! (fn []
                                     (when-some [recent-project (first (ui/selection recent-projects-list))]
                                       (close-dialog-and-open-project! (:project-file recent-project) false)))]
        (ui/on-action! open-from-disk-button (partial show-open-from-disk-dialog! last-opened-project-directory close-dialog-and-open-project!))
        (ui/on-action! open-selected-project-button (fn [_] (open-selected-project!)))
        (ui/bind-presence! empty-recent-projects-list-overlay (Bindings/isEmpty (.getItems recent-projects-list)))
        (ui/bind-enabled-to-selection! open-selected-project-button recent-projects-list)
        (doto recent-projects-list
          (.setFixedCellSize 56.0)
          (ui/items! recent-projects)
          (ui/cell-factory! (fn [recent-project]
                              {:graphic (make-recent-project-entry recent-project)}))
          (.setOnMouseClicked (ui/event-handler event
                                (when (= 2 (.getClickCount ^MouseEvent event))
                                  (open-selected-project!)))))))))

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
  ^Parent [new-project-location-directory download-template! welcome-settings]
  (doto (ui/load-fxml "welcome/new-project-pane.fxml")
    (ui/with-controls [^ButtonBase create-new-project-button new-project-location-field ^TextField new-project-title-field template-categories ^ListVew template-list]
      (setup-location-field! new-project-location-field "Select New Project Location" new-project-location-directory)
      (.bind (location-field-title-property new-project-location-field) (.textProperty new-project-title-field))
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
      (ui/bind-enabled-to-selection! create-new-project-button template-list)
      (ui/on-action! create-new-project-button
                     (fn [_]
                       (let [project-template (first (ui/selection template-list))
                             project-title (ui/text new-project-title-field)
                             project-location (location-field-location new-project-location-field)]
                         (cond
                           (string/blank? project-title)
                           (dialogs/make-message-box "No Project Title"
                                                     "You must specify a title for the project.")

                           (not= project-title (string/trim project-title))
                           (dialogs/make-message-box "Invalid Project Title"
                                                     "Whitespace is not allowed around the project title.")

                           (and (.exists project-location)
                                (not (fs/empty-directory? project-location)))
                           (dialogs/make-message-box "Conflicting Project Location"
                                                     "A non-empty folder already exists at the chosen location.")

                           :else
                           (download-template! (:name project-template) (:zip-url project-template) (:skip-root? project-template) project-location project-title))))))))

;; -----------------------------------------------------------------------------
;; Import project pane
;; -----------------------------------------------------------------------------

(defn- dashboard-project-name
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
    (make-project-entry name description last-updated matching-indices)))

(defn- make-import-project-pane
  ^Parent [new-project-location-directory clone-project! dashboard-client]
  (doto (ui/load-fxml "welcome/import-project-pane.fxml")
    (ui/with-controls [cancel-sign-in-button copy-sign-in-url-button create-account-button ^ListView dashboard-projects-list empty-dashboard-projects-list-overlay ^TextField filter-field import-project-button import-project-location-field ^TextField import-project-folder-field sign-in-button state-not-signed-in state-sign-in-browser-open state-signed-in]
      (let [sign-in-state-property ^SimpleObjectProperty (:sign-in-state-property dashboard-client)
            dashboard-projects-atom (atom nil)
            reload-dashboard-projects! (fn []
                                         (future
                                           (error-reporting/catch-all!
                                             (reset! dashboard-projects-atom (fetch-dashboard-projects dashboard-client)))))
            refresh-dashboard-projects-list! (fn [filter-text dashboard-projects]
                                               (ui/items! dashboard-projects-list
                                                          (filter-dashboard-projects filter-text dashboard-projects)))]

        ;; Show / hide various elements based on state.
        (ui/bind-presence! state-not-signed-in (Bindings/equal :not-signed-in sign-in-state-property))
        (ui/bind-presence! state-sign-in-browser-open (Bindings/equal :browser-open sign-in-state-property))
        (ui/bind-presence! state-signed-in (Bindings/equal :signed-in sign-in-state-property))
        (ui/bind-presence! filter-field (Bindings/equal :signed-in sign-in-state-property))
        (ui/bind-presence! empty-dashboard-projects-list-overlay (Bindings/isEmpty (.getItems dashboard-projects-list)))

        ;; Configure location field.
        (setup-location-field! import-project-location-field "Select Import Location" new-project-location-directory)
        (.bind (location-field-title-property import-project-location-field) (.textProperty import-project-folder-field))

        ;; Configure simple button actions.
        (ui/on-action! sign-in-button (fn [_] (begin-sign-in! dashboard-client)))
        (ui/on-action! create-account-button (fn [_] (ui/open-url "https://www.defold.com")))
        (ui/on-action! cancel-sign-in-button (fn [_] (cancel-sign-in! dashboard-client)))
        (ui/on-action! copy-sign-in-url-button (fn [_] (copy-sign-in-url! dashboard-client (Clipboard/getSystemClipboard))))

        ;; Configure Import Project button.
        (doto import-project-button
          (ui/bind-enabled-to-selection! dashboard-projects-list)
          (ui/on-action! (fn [_]
                           (let [dashboard-project (first (ui/selection dashboard-projects-list))
                                 project-title (:name dashboard-project)
                                 project-folder (ui/text import-project-folder-field)
                                 clone-directory (location-field-location import-project-location-field)]
                             (cond
                               (string/blank? project-folder)
                               (dialogs/make-message-box "No Destination Folder"
                                                         "You must specify a destination folder for the project.")

                               (not= project-folder (string/trim project-folder))
                               (dialogs/make-message-box "Invalid Destination Folder"
                                                         "Whitespace is not allowed around the folder name.")

                               (and (.exists clone-directory)
                                    (not (fs/empty-directory? clone-directory)))
                               (dialogs/make-message-box "Conflicting Import Location"
                                                         "A non-empty folder already exists at the chosen location.")

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

        (when (= :signed-in (sign-in-state sign-in-state-property))
          (reload-dashboard-projects!))))))

;; -----------------------------------------------------------------------------
;; Automatic updates
;; -----------------------------------------------------------------------------

(defn- install-pending-update-check-timer! [^Stage stage update-link update-context]
  (let [update-visibility! (fn []
                             (let [update (updater/pending-update update-context)
                                   update-exists? (and (some? update)
                                                       (not= update (system/defold-editor-sha1)))]
                               (ui/visible! update-link update-exists?)
                               update-exists?))]
    ;; Start checking for updates. On the Welcome screen we stop polling once we
    ;; found an update. If we find an update immediately, we don't even need to
    ;; install the polling timer.
    (when-not (update-visibility!)
      (let [tick-fn (fn [^AnimationTimer timer _]
                      (when (update-visibility!)
                        (.stop timer)))
            timer (ui/->timer 0.1 "pending-update-check" tick-fn)]
        (.addEventHandler stage WindowEvent/WINDOW_SHOWN (ui/event-handler event (ui/timer-start! timer)))
        (.addEventHandler stage WindowEvent/WINDOW_HIDING (ui/event-handler event (ui/timer-stop! timer)))))))

(defn- init-pending-update-indicator! [stage update-link update-context]
  (install-pending-update-check-timer! stage update-link update-context)
  (ui/on-action! update-link
                 (fn [_]
                   (when (dialogs/make-pending-update-dialog stage)
                     (when (updater/install-pending-update! update-context (io/file (system/defold-resourcespath)))
                       (updater/restart!))))))

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
         last-opened-project-directory (last-opened-project-directory prefs)
         new-project-location-directory (new-project-location-directory last-opened-project-directory)
         close-dialog-and-open-project! (fn [^File project-file newly-created?]
                                          (when (fs/existing-file? project-file)
                                            (set-last-opened-project-directory! prefs (.getParentFile project-file))
                                            (ui/close! stage)

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
                                      (ui/run-later
                                        (hide-progress! root)
                                        (cond
                                          (instance? UnknownHostException error)
                                          (dialogs/make-message-box "No Internet Connection"
                                                                    "You must be connected to the internet to download project content.")

                                          (instance? SocketException error)
                                          (dialogs/make-message-box "Host Unreachable"
                                                                    "A firewall might be blocking network connections.")

                                          (instance? SocketTimeoutException error)
                                          (dialogs/make-message-box "Host Not Responding"
                                                                    "The connection timed out.")
                                          :else
                                          (error-reporting/report-exception! error))))))))
         dashboard-client (make-dashboard-client prefs)
         sign-in-state-property ^SimpleObjectProperty (:sign-in-state-property dashboard-client)
         left-pane (.lookup root "#left-pane")
         pane-buttons-container (.lookup left-pane "#pane-buttons-container")
         sign-out-button (.lookup left-pane "#sign-out-button")
         update-link (.lookup left-pane "#update-link")
         pane-buttons-toggle-group (ToggleGroup.)
         pane-buttons [(make-pane-button "HOME" (make-home-pane last-opened-project-directory close-dialog-and-open-project! recent-projects))
                       (make-pane-button "NEW PROJECT" (make-new-project-pane new-project-location-directory download-template! welcome-settings))
                       (make-pane-button "IMPORT PROJECT" (make-import-project-pane new-project-location-directory clone-project! dashboard-client))]]

     ;; Add Defold logo SVG paths.
     (doseq [pane (map pane-button->pane pane-buttons)]
       (add-defold-logo-svg-paths! pane))

     ;; Make ourselves the main stage.
     (ui/set-main-stage stage)

     ;; Ensure any server started by the dashboard client is shut down when the stage is closed.
     (ui/on-closed! stage (fn [_] (shutdown-dashboard-client! dashboard-client)))

     ;; Install pending update check.
     (when (some? update-context)
       (init-pending-update-indicator! stage update-link update-context))

     ;; Add the pane buttons to the left panel and configure them to toggle between the panes.
     (doseq [^RadioButton pane-button pane-buttons]
       (.setToggleGroup pane-button pane-buttons-toggle-group)
       (ui/add-child! pane-buttons-container pane-button))

     (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                 (fn [_ _ selected-pane-button]
                   (let [pane (pane-button->pane selected-pane-button)]
                     (ui/with-controls root [dialog-contents]
                       (ui/children! dialog-contents [left-pane pane])))))

     ;; Select the home pane button.
     (.selectToggle pane-buttons-toggle-group (first pane-buttons))

     ;; Configure the sign-out button.
     (doto sign-out-button
       (ui/bind-presence! (Bindings/equal :signed-in sign-in-state-property))
       (ui/on-action! (fn [_] (sign-out! dashboard-client))))

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
