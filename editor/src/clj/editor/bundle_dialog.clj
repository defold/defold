(ns editor.bundle-dialog
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [editor.bundle :as bundle]
            [editor.system :as system]
            [editor.ui :as ui])
  (:import [java.io File]
           [java.util Collection List]
           [javafx.scene Scene]
           [javafx.scene.control Button CheckBox ChoiceBox Label TextField]
           [javafx.scene.layout ColumnConstraints GridPane HBox Priority VBox]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter Window]
           [javafx.util StringConverter]
           [javafx.util.converter NumberStringConverter]))

(set! *warn-on-reflection* true)

(defn- query-file!
  ^File [title filter-descs ^File initial-file ^Window owner-window]
  (let [chooser (FileChooser.)
        initial-directory (some-> initial-file .getParentFile)
        initial-file-name (some-> initial-file .getName)
        extension-filters (map (fn [filter-desc]
                                 (let [description ^String (first filter-desc)
                                       extensions ^List (vec (rest filter-desc))]
                                   (FileChooser$ExtensionFilter. description extensions)))
                               filter-descs)]
    (when (and (some? initial-directory) (.exists initial-directory))
      (.setInitialDirectory chooser initial-directory))
    (when (some? (not-empty initial-file-name))
      (.setInitialFileName chooser initial-file-name))
    (.addAll (.getExtensionFilters chooser) ^Collection extension-filters)
    (.setTitle chooser title)
    (.showOpenDialog chooser owner-window)))

(defn- query-directory!
  ^File [title ^File initial-directory ^Window owner-window]
  (let [chooser (DirectoryChooser.)]
    (when (and (some? initial-directory) (.exists initial-directory))
      (.setInitialDirectory chooser initial-directory))
    (.setTitle chooser title)
    (.showDialog chooser owner-window)))

(defn- get-file
  ^File [^TextField text-field]
  (when-let [path (not-empty (.getText text-field))]
    (io/file path)))

(defn- set-file! [^TextField text-field ^File file]
  (.setText text-field (if (some? file)
                         (.getAbsolutePath file)
                         "")))

(defn- existing-file? [^File file]
  (and (some? file) (.isFile file)))

(defn- label!
  ^HBox [label-text control]
  (assert (string? (not-empty label-text)))
  (HBox/setHgrow control Priority/ALWAYS)
  (let [label (doto (Label. label-text) (ui/add-style! "field-label"))]
    (doto (HBox.)
      (ui/children! [label control]))))

(defn- make-file-field
  ^GridPane [refresh! owner-window text-field-css-id title-text filter-descs]
  (assert (fn? refresh!))
  (assert (some? owner-window))
  (assert (string? (not-empty title-text)))
  (assert (seq filter-descs))
  (assert (every? seq filter-descs))
  (let [text-field (doto (TextField.)
                     (.setId text-field-css-id)
                     (GridPane/setFillWidth true)
                     (GridPane/setConstraints 0 0)
                     (ui/on-action! refresh!)
                     (ui/auto-commit! refresh!))
        browse-button (doto (Button. "\u2026") ; "..." (HORIZONTAL ELLIPSIS)
                        (GridPane/setConstraints 1 0)
                        (ui/add-style! "button-small")
                        (ui/on-action! (fn [_]
                                         (when-let [file (query-file! title-text filter-descs (get-file text-field) owner-window)]
                                           (set-file! text-field file)))))
        container (doto (GridPane.)
                    (ui/add-style! "composite-property-control-container")
                    (ui/children! [text-field browse-button]))]
    (doto (.getColumnConstraints container)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS)))
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER))))
    container))

(defprotocol BundleOptionsPresenter
  (make-views [this owner-window])
  (get-options [this])
  (set-options! [this options]))

(defn- make-presenter-refresher [presenter]
  (assert (satisfies? BundleOptionsPresenter presenter))
  (let [refresh-in-progress (volatile! false)]
    (fn refresh! [_]
      (when-not @refresh-in-progress
        (vreset! refresh-in-progress true)
        (try
          (set-options! presenter (get-options presenter))
          (finally
            (vreset! refresh-in-progress false)))))))

;; -----------------------------------------------------------------------------
;; Generic
;; -----------------------------------------------------------------------------

(defn- make-generic-headers [header description]
  (doto (VBox.)
    (ui/add-style! "headers")
    (ui/children! [(doto (Label. header) (ui/add-style! "header-label"))
                   (doto (Label. description) (ui/add-style! "description-label"))])))

(defn- make-generic-controls [refresh!]
  (assert (fn? refresh!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "generic")
    (ui/children! [(doto (CheckBox. "Release mode") (.setId "release-mode-check-box") (ui/on-action! refresh!))
                   (doto (CheckBox. "Generate build report") (.setId "generate-build-report-check-box") (ui/on-action! refresh!))
                   (doto (CheckBox. "Publish Live Update content") (.setId "publish-live-update-content-check-box") (ui/on-action! refresh!))])))

(defn- default-generic-options []
  {:release-mode? true
   :generate-build-report? false
   :publish-live-update-content? false})

(defn- get-generic-options [view]
  (ui/with-controls view [^CheckBox release-mode-check-box
                          ^CheckBox generate-build-report-check-box
                          ^CheckBox publish-live-update-content-check-box]
    {:release-mode? (.isSelected release-mode-check-box)
     :generate-build-report? (.isSelected generate-build-report-check-box)
     :publish-live-update-content? (.isSelected publish-live-update-content-check-box)}))

(defn- set-generic-options! [view options]
  (ui/with-controls view [^CheckBox release-mode-check-box
                          ^CheckBox generate-build-report-check-box
                          ^CheckBox publish-live-update-content-check-box]
    (.setSelected release-mode-check-box (:release-mode? options))
    (.setSelected generate-build-report-check-box (:generate-build-report? options))
    (.setSelected publish-live-update-content-check-box (:publish-live-update-content? options))))

(deftype GenericBundleOptionsPresenter [view title]
  BundleOptionsPresenter
  (make-views [this _owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers title "Set build options")
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (get-generic-options view))
  (set-options! [_this options]
    (set-generic-options! view options)))

;; -----------------------------------------------------------------------------
;; Generic, architecture-concerned
;; -----------------------------------------------------------------------------

(defn- make-architecture-controls [refresh!]
  (assert (fn? refresh!))
  (let [architecture-choice-box (doto (ChoiceBox. (ui/observable-list [32 64]))
                                  (.setId "architecture-choice-box")
                                  (.setConverter (NumberStringConverter. "#-bit"))
                                  (ui/on-action! refresh!))]
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "architecture")
      (ui/children! [(label! "Architecture" architecture-choice-box)]))))

(defn- default-architecture-options []
  {:arch-bits (if (= (system/os-arch) "x86")
                32
                64)})

(defn- get-architecture-options [view]
  (ui/with-controls view [architecture-choice-box]
    {:arch-bits (ui/value architecture-choice-box)}))

(defn- set-architecture-options! [view options]
  (ui/with-controls view [architecture-choice-box]
    (ui/value! architecture-choice-box (:arch-bits options))))

(deftype ArchitectureConcernedBundleOptionsPresenter [view title]
  BundleOptionsPresenter
  (make-views [this _owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers title "Set build options")
       (make-architecture-controls refresh!)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge (get-generic-options view)
           (get-architecture-options view)))
  (set-options! [_this options]
    (set-generic-options! view options)
    (set-architecture-options! view options)))

;; -----------------------------------------------------------------------------
;; Android
;; -----------------------------------------------------------------------------

(defn- make-android-controls [refresh! owner-window]
  (assert (fn? refresh!))
  (let [make-file-field (partial make-file-field refresh! owner-window)
        certificate-text-field (make-file-field "certificate-text-field" "Choose Certificate" [["Certificates (*.pem)" "*.pem"]])
        private-key-text-field (make-file-field "private-key-text-field" "Choose Private Key" [["Private Keys (*.pk8)" "*.pk8"]])]
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "android")
      (ui/children! [(label! "Certificate" certificate-text-field)
                     (label! "Private key" private-key-text-field)]))))

(defn- default-android-options []
  {:certificate nil
   :private-key nil})

(defn- get-android-options [view]
  (ui/with-controls view [certificate-text-field private-key-text-field]
    {:certificate (get-file certificate-text-field)
     :private-key (get-file private-key-text-field)}))

(defn- set-android-options! [view options]
  (let [certificate (:certificate options)
        private-key (:private-key options)]
    (ui/with-controls view [certificate-text-field private-key-text-field ok-button]
      (set-file! certificate-text-field certificate)
      (set-file! private-key-text-field private-key)
      (ui/enable! ok-button (and (existing-file? certificate)
                                 (existing-file? private-key))))))

(deftype AndroidBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Package Android Application" "Set certificate and private key (leave fields blank to sign APK with an auto-generated debug certificate)")
       (make-android-controls refresh! owner-window)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge (get-generic-options view)
           (get-android-options view)))
  (set-options! [_this options]
    (set-generic-options! view options)
    (set-android-options! view options)))

;; -----------------------------------------------------------------------------
;; iOS
;; -----------------------------------------------------------------------------

(defn- make-ios-controls [refresh! owner-window]
  (assert (fn? refresh!))
  (let [provisioning-profile-text-field (make-file-field refresh! owner-window "provisioning-profile-text-field" "Choose Provisioning Profile" [["Provisioning Profiles (*.mobileprovision)" "*.mobileprovision"]])
        code-signing-identities (concat [[nil "None"]] (bundle/find-identities))
        code-signing-identity-ids (mapv first code-signing-identities)
        code-signing-identity-names-by-id (into {} code-signing-identities)
        code-signing-identity-ids-by-name (set/map-invert code-signing-identity-names-by-id)
        code-signing-identity-choice-box (doto (ChoiceBox. (ui/observable-list code-signing-identity-ids))
                                           (.setId "code-signing-identity-choice-box")
                                           (.setMaxWidth Double/MAX_VALUE) ; Required to fill available space.
                                           (ui/on-action! refresh!)
                                           (.setConverter (proxy [StringConverter] []
                                                            (toString [identity-id]
                                                              (code-signing-identity-names-by-id identity-id))
                                                            (fromString [identity-name]
                                                              (code-signing-identity-ids-by-name identity-name)))))]
    ;; Selecting nil won't update the ChoiceBox since it thinks it is
    ;; already nil to begin with. Select "None" here as a workaround.
    (.selectFirst (.getSelectionModel code-signing-identity-choice-box))
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "ios")
      (ui/children! [(label! "Code signing identity" code-signing-identity-choice-box)
                     (label! "Provisioning profile" provisioning-profile-text-field)]))))

(defn- default-ios-options []
  {:code-signing-identity nil
   :provisioning-profile nil})

(defn- get-ios-options [view]
  (ui/with-controls view [code-signing-identity-choice-box provisioning-profile-text-field]
    {:code-signing-identity (ui/value code-signing-identity-choice-box)
     :provisioning-profile (get-file provisioning-profile-text-field)}))

(defn- set-ios-options! [view options]
  (ui/with-controls view [code-signing-identity-choice-box provisioning-profile-text-field ok-button]
    (let [code-signing-identity (:code-signing-identity options)
          provisioning-profile (:provisioning-profile options)]
      (ui/with-controls view [code-signing-identity-choice-box provisioning-profile-text-field ok-button]
        (ui/value! code-signing-identity-choice-box code-signing-identity)
        (set-file! provisioning-profile-text-field provisioning-profile)
        (ui/enable! ok-button (and (some? code-signing-identity)
                                   (existing-file? provisioning-profile)))))))

(deftype IOSBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Package iOS Application" "Set build options")
       (make-ios-controls refresh! owner-window)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge (get-generic-options view)
           (get-ios-options view)))
  (set-options! [_this options]
    (set-generic-options! view options)
    (set-ios-options! view options)))

;; -----------------------------------------------------------------------------

(defmulti bundle-options-presenter (fn [_view platform] platform))
(defmethod bundle-options-presenter :default [_view platform] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-options-presenter :android [view _platform] (AndroidBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :html5   [view _platform] (GenericBundleOptionsPresenter. view "Package HTML5 Application"))
(defmethod bundle-options-presenter :ios     [view _platform] (IOSBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :linux   [view _platform] (ArchitectureConcernedBundleOptionsPresenter. view "Package Linux Application"))
(defmethod bundle-options-presenter :macos   [view _platform] (GenericBundleOptionsPresenter. view "Package macOS Application"))
(defmethod bundle-options-presenter :windows [view _platform] (ArchitectureConcernedBundleOptionsPresenter. view "Package Windows Application"))

(defn- default-build-options []
  (merge {:output-directory nil}
         (default-generic-options)
         (default-architecture-options)
         (default-android-options)
         (default-ios-options)))

(defn show-bundle-dialog! [platform build-options owner-window bundle!]
  (let [defaults (default-build-options)
        build-options (merge defaults (select-keys build-options (keys defaults)))
        stage (doto (ui/make-dialog-stage owner-window) (ui/title! "Package Application"))
        root (doto (VBox.)
               (ui/add-style! "bundle-dialog")
               (ui/add-style! (name platform))
               (ui/apply-css!))
        presenter (bundle-options-presenter root platform)]

    ;; Presenter views.
    (doseq [view (make-views presenter stage)]
      (ui/add-child! root view))

    ;; Button bar.
    (let [ok-button (doto (Button. "Package") (.setId "ok-button"))
          buttons (doto (HBox.) (ui/add-style! "buttons"))]
      (ui/add-child! buttons ok-button)
      (ui/add-child! root buttons)
      (ui/on-action! ok-button
                     (fn on-ok! [_]
                       (let [build-options (get-options presenter)]
                         (ui/close! stage)
                         (if-let [output-directory (query-directory! "Output Directory" (:output-directory build-options) owner-window)]
                           (let [build-options (assoc build-options :output-directory output-directory)]
                             (bundle! build-options))
                           (show-bundle-dialog! platform build-options owner-window bundle!))))))

    ;; Refresh from build options.
    (set-options! presenter build-options)

    ;; Show dialog.
    (let [scene (Scene. root)]
      (.setScene stage scene)
      (ui/show! stage))))
