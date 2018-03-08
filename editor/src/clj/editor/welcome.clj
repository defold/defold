(ns editor.welcome
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [schema.core :as s])
  (:import (java.net MalformedURLException URL)
           (java.io PushbackReader)
           (javafx.event Event)
           (javafx.scene Node Scene Parent)
           (javafx.scene.control ToggleGroup RadioButton)
           (javafx.scene.image ImageView Image)
           (javafx.scene.layout HBox StackPane VBox)
           (javafx.scene.text Text TextFlow)
           (javafx.scene.shape Rectangle)))

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

(defn- make-home-pane
  ^Parent []
  (let [pane (ui/load-fxml "welcome/home-pane.fxml")]
    pane))

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

(defn- browse-field-text
  ^String [^Node browse-field]
  (ui/text (.lookup browse-field "TextField")))

(defn- make-new-project-pane
  ^Parent [welcome-settings]
  (doto (ui/load-fxml "welcome/new-project-pane.fxml")
    (ui/with-controls [template-list title-field location-browse-field submit-button]
      (setup-directory-browse-field! "Select New Project Location" location-browse-field)
      (ui/on-action! submit-button
                     (fn [_]
                       (let [project-title (ui/text title-field)
                             project-location (browse-field-text location-browse-field)
                             project-template (first (ui/selection template-list))]
                         (dialogs/make-alert-dialog (pr-str {:project-title project-title
                                                             :project-location project-location
                                                             :project-template project-template})))))
      (doto template-list
        (ui/items! (get-in welcome-settings [:new-project :templates]))
        (ui/cell-factory! (fn [project-template]
                            {:graphic (make-template-entry project-template)}))))))

(defn- make-open-project-pane
  ^Parent []
  (let [pane (ui/load-fxml "welcome/open-project-pane.fxml")]
    pane))

(defn- make-pane-button [label pane]
  (doto (RadioButton.)
    (ui/user-data! :pane pane)
    (ui/add-style! "pane-button")
    (ui/text! label)))

(defn- pane-button->pane
  ^Parent [pane-button]
  (ui/user-data pane-button :pane))

(defn open-welcome-dialog! []
  (let [welcome-settings (load-welcome-settings "welcome/welcome.edn")
        root (ui/load-fxml "welcome/welcome-dialog.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        left-pane ^Parent (.lookup root "#left-pane")
        pane-buttons-toggle-group (ToggleGroup.)
        pane-buttons [(make-pane-button "HOME" (make-home-pane))
                      (make-pane-button "NEW PROJECT" (make-new-project-pane welcome-settings))
                      (make-pane-button "OPEN PROJECT" (make-open-project-pane))]]

    ;; TEMP HACK, REMOVE!
    ;; Reload stylesheets.
    (let [styles (vec (.getStylesheets root))]
      (.forget (com.sun.javafx.css.StyleManager/getInstance) scene)
      (.setAll (.getStylesheets root) ^java.util.Collection styles))

    ;; Add the pane buttons to the left panel and configure them to toggle between the panes.
    (doseq [pane-button pane-buttons]
      (.setToggleGroup pane-button pane-buttons-toggle-group)
      (ui/add-child! left-pane pane-button))

    (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                (fn [_ _ selected-pane-button]
                  (let [pane (pane-button->pane selected-pane-button)]
                    (ui/children! root [left-pane pane]))))

    ;; Select the home pane button.
    (.selectToggle pane-buttons-toggle-group (second pane-buttons))

    ;; Show the dialog.
    (.setScene stage scene)
    (ui/show! stage)))
