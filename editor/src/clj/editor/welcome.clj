;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.welcome
  (:require [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.text :as fx.text]
            [cljfx.fx.text-flow :as fx.text-flow]
            [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.string :as string]
            [editor.analytics :as analytics]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.game-project-core :as game-project-core]
            [editor.icons :as icons]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.ui.updater :as ui.updater]
            [schema.core :as s]
            [util.coll :as coll]
            [util.net :as net]
            [util.time :as time])
  (:import [clojure.lang ExceptionInfo]
           [com.defold.control UpdatableListCell]
           [java.io File FileOutputStream PushbackReader]
           [java.net MalformedURLException SocketException SocketTimeoutException URL UnknownHostException]
           [java.time Instant]
           [java.util Collection]
           [java.util.zip ZipInputStream]
           [javafx.beans.property StringProperty]
           [javafx.beans.value ChangeListener]
           [javafx.event Event]
           [javafx.geometry Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button ButtonBase ComboBox Hyperlink Label ListCell ListView OverrunStyle ProgressBar RadioButton TextArea TextField ToggleGroup]
           [javafx.scene.image Image ImageView]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [javafx.scene.shape Rectangle]
           [javafx.scene.text Text TextFlow]
           [javafx.util Callback]
           [javax.net.ssl SSLException]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private last-opened-project-directory-prefs-key [:welcome :last-opened-project-directory])
(defonce ^:private recent-projects-prefs-key [:welcome :recent-projects])

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
   (s/optional-key :bundle) s/Bool
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

(defn- load-edn [edn-resource]
  (with-open [reader (PushbackReader. (io/reader edn-resource))]
    (edn/read reader)))

(defn- load-welcome-settings-resource [resource]
  (try
    [(s/validate WelcomeSettings (load-edn resource)) nil]
    (catch Exception error
      [nil error])))

(defn- load-welcome-settings-file [^File file]
  (if (.exists file)
    (try
      [(s/validate WelcomeSettings (load-edn file)) nil]
      (catch Exception error
        [nil error]))
    [{} nil]))



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
            (settings-core/settings->str game-project-core/meta-settings :multi-line-list))))

;; -----------------------------------------------------------------------------
;; Preferences management
;; -----------------------------------------------------------------------------

(defn- last-opened-project-directory
  ^File [prefs]
  (when-some [directory (io/as-file (prefs/get prefs last-opened-project-directory-prefs-key))]
    directory))

(defn- set-last-opened-project-directory!
  ^File [prefs ^File directory]
  (prefs/set! prefs last-opened-project-directory-prefs-key (.getAbsolutePath directory)))

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

(defn- recent-projects
  "Returns a sequence of recently opened projects. Project files that no longer
  exist will be filtered out. The projects are returned in descending order with
  the most recently opened project first."
  [prefs]
  (->> (prefs/get prefs recent-projects-prefs-key)
       (into [] xform-timestamps-by-path->recent-projects)
       (sort-by :last-opened coll/descending-order)))

(defn add-recent-project!
  "Updates the recent projects list in the preferences to include a new entry
  for the specified project-file. Uses the current time for the timestamp."
  [prefs ^File project-file]
  (let [recent-projects (recent-projects prefs)
        timestamps-by-path (assoc (into {} xform-recent-projects->timestamps-by-path recent-projects)
                             (.getAbsolutePath project-file) (str (Instant/now)))]
    (prefs/set! prefs recent-projects-prefs-key timestamps-by-path)))

(defn remove-recent-project! [prefs ^File project-file]
  (when-let [recent-projects-map (prefs/get prefs recent-projects-prefs-key)]
    (prefs/set! prefs recent-projects-prefs-key (dissoc recent-projects-map (.getAbsolutePath project-file)))))

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

(defn- find-bundled-proj-zip!
  ^File [project-title]
  (io/resource (str "template-projects/" project-title ".zip")))

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

(defn- on-location-field-browse-button-click! [localization dialog-title ^Event event]
  (let [^Node button (.getSource event)
        location-field (.getParent button)]
    (ui/with-controls location-field [directory-text]
      (let [scene (.getScene button)
            window (.getWindow scene)
            initial-directory (some-> directory-text ui/text not-empty io/as-file)
            initial-directory (when (some-> initial-directory (.exists)) initial-directory)]
        (when-some [directory (dialogs/make-directory-dialog (localization (localization/message dialog-title)) initial-directory window)]
          (ui/text! directory-text (.getAbsolutePath directory)))))))

(defn- setup-location-field! [^HBox location-field localization dialog-title ^File directory]
  (doto location-field
    (ui/add-style! "location-field")
    (ui/children! [(doto (HBox.)
                     (HBox/setHgrow Priority/ALWAYS)
                     (ui/add-style! "path-field")
                     (ui/children! [(doto (Label.)
                                      (HBox/setHgrow Priority/ALWAYS)
                                      (.setId "directory-text")
                                      (.setMaxWidth Double/MAX_VALUE)
                                      (.setTextOverrun OverrunStyle/LEADING_ELLIPSIS)
                                      (.setEllipsisString "…")
                                      (ui/add-styles! ["path-element" "explicit"])
                                      (ui/text! (or (some-> directory .getAbsolutePath) "")))
                                    (doto (Label. File/separator)
                                      (HBox/setHgrow Priority/NEVER)
                                      (ui/add-styles! ["path-element" "implicit"]))
                                    (doto (Label.)
                                      (HBox/setHgrow Priority/NEVER)
                                      (.setMinWidth Region/USE_COMPUTED_SIZE)
                                      (.setTextOverrun OverrunStyle/ELLIPSIS)
                                      (.setEllipsisString "…")
                                      (.setId "title-text")
                                      (ui/add-styles! ["path-element" "implicit"]))]))
                   (doto (Button. "\u2022 \u2022 \u2022") ; "* * *" (BULLET)
                     (ui/on-action! (partial on-location-field-browse-button-click! localization dialog-title)))])))

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

(defn- show-open-from-disk-dialog! [^File last-opened-project-directory close-dialog-and-open-project! localization ^Event event]
  (let [^Node button (.getSource event)
        scene (.getScene button)
        window (.getWindow scene)
        filter-descs [[(localization (localization/message "welcome.open-from-disk.dialog.filter.defold")) "*.project"]]
        initial-file (some-> last-opened-project-directory (io/file "game.project"))]
    (when-some [project-file (dialogs/make-file-dialog (localization (localization/message "welcome.open-from-disk.dialog.title")) filter-descs initial-file window)]
      (close-dialog-and-open-project! project-file false))))

(defn- timestamp-label [timestamp localization]
  (doto (Label.)
    (ui/add-style! "timestamp")
    (localization/localize! localization (if (some? timestamp)
                                           (time/vague-description timestamp)
                                           (localization/message "time.vague.unknown")))))

(defn- title-label [title matching-indices]
  (doto (fuzzy-choices/make-matched-text-flow title matching-indices)
    (ui/add-style! "title")))

(defn- make-project-entry
  ^Node [^String title ^String description ^Instant timestamp matching-indices on-remove localization]
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
                (.setGraphic (icons/get-image-view "icons/32/Icons_S_01_SmallClose.png" 10)))
              (timestamp-label timestamp localization)]))
         (timestamp-label timestamp localization))])))

(defn- make-recent-project-entry
  ^Node [{:keys [^File project-file ^Instant last-opened ^String title] :as _recent-project}
         recent-projects-list-view
         prefs
         localization]
  (let [on-remove #(do
                     (remove-recent-project! prefs project-file)
                     (ui/items! recent-projects-list-view (recent-projects prefs)))]
    (make-project-entry title (.getParent project-file) last-opened nil on-remove localization)))

(defn- make-home-pane
  ^Parent [last-opened-project-directory show-new-project-pane! close-dialog-and-open-project! prefs localization]
  (let [home-pane (ui/load-fxml "welcome/home-pane.fxml")
        open-from-disk-buttons (.lookupAll home-pane "#open-from-disk-button")
        show-open-from-disk-dialog! (partial show-open-from-disk-dialog! last-opened-project-directory close-dialog-and-open-project! localization)]
    (ui/with-controls home-pane [^ListView recent-projects-list
                                 ^Node state-empty-recent-projects-list
                                 ^Node state-non-empty-recent-projects-list
                                 ^ButtonBase open-selected-project-button
                                 ^ButtonBase show-new-project-pane-button
                                 recent-projects-label
                                 welcome-text]
      (localization/localize! recent-projects-label localization (localization/message "welcome.recent-projects"))
      (localization/localize! welcome-text localization (localization/message "welcome.greeting"))
      (let [open-selected-project! (fn []
                                     (when-some [recent-project (first (ui/selection recent-projects-list))]
                                       (close-dialog-and-open-project! (:project-file recent-project) false)))]
        (doseq [open-from-disk-button open-from-disk-buttons]
          (localization/localize! open-from-disk-button localization (localization/message "welcome.button.open-from-disk"))
          (ui/on-action! open-from-disk-button show-open-from-disk-dialog!))
        (ui/on-action! show-new-project-pane-button (fn [_] (show-new-project-pane!)))
        (localization/localize! show-new-project-pane-button localization (localization/message "welcome.button.create-new-project"))
        (ui/on-action! open-selected-project-button (fn [_] (open-selected-project!)))
        (localization/localize! open-selected-project-button localization (localization/message "welcome.button.open-selected"))
        (b/bind-presence! state-empty-recent-projects-list (b/empty? (.getItems recent-projects-list)))
        (b/bind-presence! state-non-empty-recent-projects-list (b/not (b/empty? (.getItems recent-projects-list))))
        (b/bind-enabled-to-selection! open-selected-project-button recent-projects-list)
        (doto recent-projects-list
          (.setFixedCellSize 56.0)
          (ui/items! (recent-projects prefs))
          (ui/cell-factory!
            (fn [recent-project]
              {:graphic (make-recent-project-entry recent-project recent-projects-list prefs localization)}))
          (.setOnMouseClicked (ui/event-handler event
                                (let [^MouseEvent mouse-event event
                                      ^Node target (.getTarget mouse-event)]
                                  (when (and (instance? ListCell target)
                                             (nil? (.getItem ^ListCell target)))
                                    (.clearSelection (.getSelectionModel recent-projects-list)))
                                  (when (and (= 2 (.getClickCount mouse-event))
                                             (not-empty (ui/selection recent-projects-list)))
                                    (open-selected-project!)))))
          (.setOnKeyPressed (ui/event-handler event
                              (when (= KeyCode/ENTER (.getCode ^KeyEvent event))
                                (open-selected-project!))))
          (when (not-empty (recent-projects prefs))
            (ui/select-index! recent-projects-list 0)
            (.requestFocus recent-projects-list)))))
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
  ^Node [localization ^String name ^String description ^String zip-url]
  (doto (VBox.)
    (ui/add-style! "description")
    (ui/children! [(doto (Text.)
                     (localization/localize! localization (localization/message name))
                     (ui/add-style! "header"))
                   (doto (TextFlow.)
                     (ui/add-child!
                       (localization/localize! (Text.) localization (localization/message description)))
                     (VBox/setVgrow Priority/ALWAYS))
                   (doto (Hyperlink. zip-url)
                     (ui/on-action! (fn [_] (ui/open-url zip-url))))])))

(defn- make-template-entry
  ^Node [project-template localization]
  (doto (HBox.)
    (ui/add-style! "template-entry")
    (ui/children! [(make-icon-view (:image project-template))
                   (make-description-view localization
                                          (:name project-template)
                                          (:description project-template)
                                          (:zip-url project-template))])))

(defn- new-project-screen-name
  ^String [^ButtonBase selected-template-category-button]
  (str "new-project-"
       (-> (.getText selected-template-category-button)
           (string/replace " " "-")
           (string/lower-case))))

(defn- init-new-project-pane!
  [^Parent root new-project-location-directory templates localization download-template!]
  (ui/with-controls root [^ButtonBase create-new-project-button
                          new-project-location-field
                          ^TextField new-project-title-field
                          ^ListView template-list
                          new-project-title-label
                          new-project-location-label]
    (.setText new-project-title-field (localization (localization/message "welcome.new-project.default-name")))
    (localization/localize! new-project-title-label localization (localization/message "welcome.new-project.title"))
    (localization/localize! new-project-location-label localization (localization/message "welcome.new-project.location"))
    (localization/localize! create-new-project-button localization (localization/message "welcome.button.create-new-project"))
    (setup-location-field! new-project-location-field localization "welcome.new-project.location.dialog.title" new-project-location-directory)
    (let [title-text-property (.textProperty new-project-title-field)
          sanitized-title-property (b/map dialogs/sanitize-folder-name title-text-property)]
      (b/bind! (location-field-title-property new-project-location-field) sanitized-title-property))
    (doto template-list
      (ui/cell-factory! (fn [project-template]
                          {:graphic (make-template-entry project-template localization)}))
      (.setOnMouseClicked (ui/event-handler event
                            (let [^MouseEvent mouse-event event
                                  ^Node target (.getTarget mouse-event)]
                              (when (and (instance? ListCell target)
                                         (nil? (.getItem ^ListCell target)))
                                (.clearSelection (.getSelectionModel template-list)))
                              (when (and (= 2 (.getClickCount mouse-event))
                                         (not-empty (ui/selection template-list)))
                                (.requestFocus new-project-title-field)))))
      (.setOnKeyPressed (ui/event-handler event
                          (when (and (= KeyCode/ENTER (.getCode ^KeyEvent event))
                                     (first (ui/selection template-list)))
                            (.requestFocus new-project-title-field)))))
    (when (some? templates)
      (ui/items! template-list templates)
      (when (seq templates)
        (ui/select-index! template-list 0)
        (when-some [selected-project-title (:name (first templates))]
          (ui/text! new-project-title-field (localization (localization/message selected-project-title))))))

    ;; Update the Title field whenever a template is selected.
    (ui/observe-selection template-list
                          (fn [_ selection]
                            (when-some [selected-project-title (:name (first selection))]
                              (ui/text! new-project-title-field (localization (localization/message selected-project-title))))))

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
              localization
              {:title (localization/message "welcome.new-project.error.no-project-title.title")
               :icon :icon/triangle-error
               :header (localization/message "welcome.new-project.error.no-project-title.header")})

            (not= project-title (string/trim project-title))
            (dialogs/make-info-dialog
              localization
              {:title (localization/message "welcome.new-project.error.invalid-project-title.title")
               :icon :icon/triangle-error
               :size :large
               :header (localization/message "welcome.new-project.error.invalid-project-title.header")})

            (and (.exists project-location)
                 (not (fs/empty-directory? project-location)))
            (dialogs/make-info-dialog
              localization
              {:title (localization/message "welcome.new-project.error.conflicting-project-location.title")
               :icon :icon/triangle-error
               :size :large
               :header (localization/message "welcome.new-project.error.conflicting-project-location.header")})

            :else
            (download-template! (:name project-template) (:zip-url project-template) (:skip-root? project-template) project-location project-title)))))))

(defn- make-new-project-pane
  (^Parent [new-project-location-directory download-template! template-category localization]
   ;; Single-category variant: hide internal category tabs and show only provided templates.
   (let [{:keys [templates] :as _tc} template-category]
     (s/validate TemplateProjectCategory template-category)
     (let [pane (ui/load-fxml "welcome/new-project-pane.fxml")]
       (init-new-project-pane! pane new-project-location-directory templates localization download-template!)
       pane))))

;; -----------------------------------------------------------------------------
;; Automatic updates
;; -----------------------------------------------------------------------------

(defn- init-pending-update-indicator! [stage update-link progress-bar progress-vbox updater localization]
  (let [render-progress! (progress/throttle-render-progress
                           (fn [progress]
                             (ui/run-later
                               (ui/visible! progress-vbox (progress/done? progress))
                               (ui/visible! progress-bar (not (progress/done? progress)))
                               (ui/render-progress-bar! progress progress-bar))))
        install-and-restart! #(ui.updater/install-and-restart! stage updater localization)]
    (ui.updater/init! stage update-link updater install-and-restart! render-progress! localization)))

;; -----------------------------------------------------------------------------
;; Welcome dialog
;; -----------------------------------------------------------------------------

(defn- make-pane-button [localization message-key pane]
  (doto (RadioButton.)
    (localization/localize! localization (localization/message message-key))
    (.setFocusTraversable false)
    (ui/user-data! :pane pane)
    (ui/add-style! "pane-button")))

(defn- pane-button->pane
  ^Parent [pane-button]
  (ui/user-data pane-button :pane))

(defn- make-left-header
  "Creates a left-pane section header with Defold sub-header styling."
  [localization message-key]
  (doto (HBox.)
    (ui/add-style! "header-pane")
    (ui/add-child! (doto (Label.)
                      (ui/add-style! "df-sub-header")
                      (localization/localize! localization (localization/message message-key))))))

(defn- show-progress!
  ^ProgressBar [^Parent root localization header-text-key template-name-key cancel-button-text-key cancel!]
  (ui/with-controls root [^ProgressBar progress-bar ^ButtonBase progress-cancel-button progress-header ^Parent progress-overlay]
    (localization/localize! progress-header localization (localization/message header-text-key {"template" (localization/message template-name-key)}))
    (localization/localize! progress-cancel-button localization (localization/message cancel-button-text-key))
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
  ([prefs localization updater open-project-fn]
   (show-welcome-dialog! prefs localization updater open-project-fn nil))
  ([prefs localization updater open-project-fn opts]
   (let [[default-welcome-settings
          default-welcome-settings-load-error] (load-welcome-settings-resource (io/resource "welcome/welcome.edn"))
         [custom-welcome-settings
          custom-welcome-settings-load-error] (load-welcome-settings-file (io/file (System/getProperty "user.home") ".defold" "welcome.edn"))

         default-categories (get-in default-welcome-settings [:new-project :categories])
         custom-categories (get-in custom-welcome-settings [:new-project :categories] [])
         welcome-settings {:new-project {:categories (concat default-categories custom-categories)}}
         welcome-settings-load-error (or default-welcome-settings-load-error custom-welcome-settings-load-error)
         root (ui/load-fxml "welcome/welcome-dialog.fxml")
         min-width 792.0
         min-height 338.0
         stage (doto (ui/make-dialog-stage)
                 (.setTitle (ui/make-title))
                 (.setResizable true))
         ;; Adapted from https://stackoverflow.com/questions/57425534/how-to-limit-how-much-the-user-can-resize-a-javafx-window
         ;; because setting minWidth/minHeight on a resizable stage does not prevent resizing the stage to a smaller size
         _ (.addListener (.widthProperty stage)
                         (reify ChangeListener
                           (changed [_ _ _ v]
                             (when (< (double v) min-width)
                               (doto stage
                                 (.setResizable false)
                                 (.setWidth min-width)
                                 (.setResizable true))))))
         _ (.addListener (.heightProperty stage)
                         (reify ChangeListener
                           (changed [_ _ _ v]
                             (when (< (double v) min-height)
                               (doto stage
                                 (.setResizable false)
                                 (.setHeight min-height)
                                 (.setResizable true))))))
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
         download-template! (fn [template-title zip-url skip-root? dest-directory project-title]
                              (let [cancelled-atom (atom false)
                                    progress-bar (show-progress! root localization "welcome.downloading-project" template-title "welcome.button.cancel-download" #(reset! cancelled-atom true))
                                    progress-callback (fn [^long done ^long total]
                                                        (when (pos? total)
                                                          (let [progress (/ (double done) (double total))]
                                                            (ui/run-later
                                                              (.setProgress progress-bar progress)))))]
                                (analytics/track-event! "welcome" "download-template" template-title)
                                (future
                                  (try
                                    (if-some [template-zip-file (or (find-bundled-proj-zip! template-title)
                                                                    (download-proj-zip! zip-url progress-callback cancelled-atom))]
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
                                            localization
                                            {:title (localization/message "welcome.new-project.error.no-internet-connection.title")
                                             :icon :icon/triangle-error
                                             :header (localization/message "welcome.new-project.error.no-internet-connection.header")})

                                          (instance? SocketException error)
                                          (dialogs/make-info-dialog
                                            localization
                                            {:title (localization/message "welcome.new-project.error.host-unreachable.title")
                                             :icon :icon/triangle-error
                                             :header (localization/message "welcome.new-project.error.host-unreachable.header")
                                             :content (.getMessage error)})

                                          (instance? SocketTimeoutException error)
                                          (dialogs/make-info-dialog
                                            localization
                                            {:title (localization/message "welcome.new-project.error.host-not-responding.title")
                                             :icon :icon/triangle-error
                                             :header (localization/message "welcome.new-project.error.host-not-responding.header")
                                             :content (.getMessage error)})

                                          (instance? SSLException error)
                                          (dialogs/make-info-dialog
                                            localization
                                            {:title (localization/message "welcome.new-project.error.ssl-connection-error.title")
                                             :icon :icon/triangle-error
                                             :header (localization/message "welcome.new-project.error.ssl-connection-error.header")
                                             :content {:fx/type fx.text-flow/lifecycle
                                                       :style-class "dialog-content-padding"
                                                       :children [{:fx/type fx.text/lifecycle
                                                                   :text (localization (localization/message "welcome.new-project.error.ssl-connection-error.common-causes"))}
                                                                  {:fx/type fx.hyperlink/lifecycle
                                                                   :text (localization (localization/message "welcome.new-project.error.ssl-connection-error.pkix-faq-entry"))
                                                                   :on-action (fn [_]
                                                                                (ui/open-url "https://github.com/defold/editor2-issues/blob/master/faq/pkixpathbuilding.md"))}
                                                                  {:fx/type fx.text/lifecycle
                                                                   :text (str "\n\n" (string/replace (.getMessage error) ": " ":\n\u00A0\u00A0"))}]}})

                                          :else
                                          (error-reporting/report-exception! error))))))))
         left-pane (.lookup root "#left-pane")
         ^ComboBox language-selector (.lookup left-pane "#language-selector")
         pane-buttons-container (.lookup left-pane "#pane-buttons-container")
         pane-buttons-toggle-group (ToggleGroup.)
         categories (vec (get-in welcome-settings [:new-project :categories]))
         category-pane-buttons (mapv (fn [category]
                                       (make-pane-button localization
                                                         (:label category)
                                                         (make-new-project-pane new-project-location-directory download-template! category localization)))
                                     categories)
         show-first-category-pane! (fn []
                                     (when-let [first-btn (first category-pane-buttons)]
                                       (.selectToggle pane-buttons-toggle-group first-btn)))
         home-pane-button (make-pane-button localization "welcome.tab.home"
                                            (make-home-pane last-opened-project-directory
                                                            show-first-category-pane!
                                                            close-dialog-and-open-project!
                                                            prefs
                                                            localization))
         pane-buttons (into [home-pane-button] category-pane-buttons)
         screen-name (fn [selected-pane-button]
                       (if (= selected-pane-button home-pane-button)
                         "home"
                         (new-project-screen-name selected-pane-button)))]

     ;; Add Defold logo SVG paths.
     (doseq [pane (map pane-button->pane pane-buttons)]
       (add-defold-logo-svg-paths! pane))

     ;; Make ourselves the main stage.
     (ui/set-main-stage stage)

     ;; Init language selector
     (.setAll (.getItems language-selector) ^Collection (localization/available-locales @localization))
     (let [locale (localization/current-locale @localization)
           ^Callback cell-factory (fn [_]
                                    (UpdatableListCell.
                                      (fn [^UpdatableListCell cell locale]
                                        (.setText cell (localization/locale-display-name locale)))))]
       (.setValue language-selector locale)
       (.setButtonCell language-selector (.call cell-factory nil))
       (.setCellFactory language-selector cell-factory))
     (.addListener (.valueProperty language-selector) ^ChangeListener #(localization/set-locale! localization %3))

     ;; Install pending update check.
     (when (some? updater)
       (ui/with-controls left-pane [update-link update-progress-bar update-progress-vbox]
         (init-pending-update-indicator! stage
                                         update-link
                                         update-progress-bar
                                         update-progress-vbox
                                         updater
                                         localization)))

     ;; Configure toggle group for all pane buttons.
     (doseq [^RadioButton pane-button pane-buttons]
       (.setToggleGroup pane-button pane-buttons-toggle-group))

     ;; Add grouped headers and pane buttons to the left panel.
     (let [open-header (make-left-header localization "welcome.header.open")
           new-project-header (make-left-header localization "welcome.header.new-project-from")
           ^RadioButton home-button home-pane-button]
       (ui/add-child! pane-buttons-container open-header)
       (ui/add-child! pane-buttons-container home-button)
       (ui/add-child! pane-buttons-container new-project-header)
       (doseq [^RadioButton pane-button category-pane-buttons]
         (ui/add-child! pane-buttons-container pane-button)))

     (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                 (fn [_ old-selected-pane-button new-selected-pane-button]
                   (when (some? old-selected-pane-button)
                     (analytics/track-screen! (screen-name new-selected-pane-button)))
                   (let [pane (pane-button->pane new-selected-pane-button)]
                     (ui/with-controls root [dialog-contents]
                       (ui/children! dialog-contents [left-pane pane])))))

     ;; Select the home pane button.
     (.selectToggle pane-buttons-toggle-group home-pane-button)

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
                                             (= KeyCode/R (.getCode key-event)))
                                    (ui/close! stage)
                                    (show-welcome-dialog! prefs localization updater open-project-fn
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
       ;; Display the error message on the first new project category pane, so we can fix and reload.
       (when (system/defold-dev?)
         (let [target-pane-button (or (first category-pane-buttons) home-pane-button)
               target-pane (pane-button->pane target-pane-button)]
           (.selectToggle pane-buttons-toggle-group target-pane-button)
           (ui/children! target-pane [(doto (TextArea.)
                                        (.setPrefHeight 10000.0)
                                        (.setEditable false)
                                        (.setStyle "-fx-font-family: 'Dejavu Sans Mono'; -fx-font-size: 90%;")
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
     (analytics/track-screen! (screen-name (.getSelectedToggle pane-buttons-toggle-group)))
     (.setScene stage scene)
     (ui/show! stage localization))))
