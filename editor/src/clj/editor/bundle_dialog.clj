(ns editor.bundle-dialog
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [editor.bundle :as bundle]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.ui :as ui])
  (:import [java.io File]
           [java.util Collection List]
           [javafx.scene Scene]
           [javafx.scene.control Button CheckBox ChoiceBox Label TextField]
           [javafx.scene.input KeyCode]
           [javafx.scene.layout ColumnConstraints GridPane HBox Priority VBox]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter Window]
           [javafx.util StringConverter]))

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
    (when (some-> initial-directory .isDirectory)
      (.setInitialDirectory chooser initial-directory))
    (.setTitle chooser title)
    (.showDialog chooser owner-window)))

(defn- query-overwrite!
  [^File existing-entry ^Window owner-window]
  (let [writable? (try
                    (and (.isDirectory existing-entry)
                         (.canWrite existing-entry))
                    (catch SecurityException _
                      false))]
    (if-not writable?
      (do (dialogs/make-alert-dialog (str "Cannot create directory at \"" (.getAbsolutePath existing-entry) "\". You might not have permission to write to that directory, or there might be a file with the same name as the directory we're trying to create."))
          false)
      (dialogs/make-confirm-dialog (str "A directory already exists at \"" (.getAbsolutePath existing-entry) "\".")
                                   {:title "Overwrite Existing Directory?"
                                    :ok-label "Overwrite"
                                    :owner-window owner-window}))))

(defn- get-file
  ^File [^TextField text-field]
  (when-let [path (not-empty (.getText text-field))]
    (io/file path)))

(defn- set-file! [^TextField text-field ^File file]
  (let [new-text (if (some? file) (.getAbsolutePath file) "")]
    (when (not= new-text (.getText text-field))
      (.setText text-field new-text))))

(defn- set-choice! [^ChoiceBox choice-box entries selected-entry]
  ;; Selecting nil won't update the ChoiceBox since it thinks it is
  ;; already nil to begin with. Select the first entry here as a workaround.
  (.setItems choice-box (ui/observable-list entries))
  (.selectFirst (.getSelectionModel choice-box))
  (ui/value! choice-box selected-entry))

(defn- existing-file? [^File file]
  (and (some? file) (.isFile file)))

(defn- existing-file-of-type? [^String ext ^File file]
  (and (existing-file? file)
       (.endsWith (.toLowerCase (.getPath file))
                  (str "." ext))))

(defn- labeled!
  ^HBox [label-text control]
  (assert (string? (not-empty label-text)))
  (HBox/setHgrow control Priority/ALWAYS)
  (let [label (doto (Label. label-text) (ui/add-style! "field-label"))]
    (doto (HBox.)
      (ui/children! [label control]))))

(defn- set-style-class! [stylable key style-classes-by-key]
  (assert (map? style-classes-by-key))
  (assert (or (nil? key) (contains? style-classes-by-key key)))
  (ui/remove-styles! stylable (vals style-classes-by-key))
  (when-let [style-class (style-classes-by-key key)]
    (ui/add-style! stylable style-class)))

(defn- set-label-status! [label [severity message]]
  (ui/text! label message)
  (set-style-class! label severity {:fatal "error"
                                    :warning "warning"
                                    :info "info"}))

(defn- set-field-status! [field [severity message]]
  (ui/tooltip! field (not-empty message))
  (set-style-class! field severity {:fatal "error"
                                    :warning "warning"
                                    :info "info"}))

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
                     (ui/prevent-auto-focus!)
                     (ui/on-action! refresh!)
                     (ui/auto-commit! refresh!))
        browse-button (doto (Button. "\u2026") ; "..." (HORIZONTAL ELLIPSIS)
                        (.setFocusTraversable false)
                        (GridPane/setConstraints 1 0)
                        (ui/add-style! "button-small")
                        (ui/on-action! (fn [event]
                                         (when-let [file (query-file! title-text filter-descs (get-file text-field) owner-window)]
                                           (set-file! text-field file)
                                           (refresh! event)))))
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

(defn- refresh-presenter! [presenter]
  (set-options! presenter (get-options presenter)))

(defn- make-presenter-refresher [presenter]
  (assert (satisfies? BundleOptionsPresenter presenter))
  (let [refresh-in-progress (volatile! false)]
    (fn refresh! [_]
      (when-not @refresh-in-progress
        (vreset! refresh-in-progress true)
        (try
          (refresh-presenter! presenter)
          (finally
            (vreset! refresh-in-progress false)))))))

(defn- get-string-pref
  ^String [prefs key]
  (some-> (prefs/get-prefs prefs key nil) not-empty))

(defn- set-string-pref!
  [prefs key ^String string]
  (prefs/set-prefs prefs key (not-empty string)))

(defn- get-file-pref
  ^File [prefs key]
  (io/file (get-string-pref prefs key)))

(defn- set-file-pref!
  [prefs key ^File file]
  (set-string-pref! prefs key (when file (.getPath file))))

;; -----------------------------------------------------------------------------
;; Generic
;; -----------------------------------------------------------------------------

(def ^:private no-issues-header-info-text "Proceed to select output folder.")

(defn- make-generic-headers
  ^VBox [header]
  (doto (VBox.)
    (ui/add-style! "headers")
    (ui/children! [(doto (Label. header) (ui/add-style! "header-label"))
                   (doto (Label. no-issues-header-info-text) (.setId "header-info-label"))])))

(defn- get-header-status [issues-by-key key-order]
  (assert (map? issues-by-key))
  (assert (sequential? key-order))
  (or (:general issues-by-key)
      (first (keep issues-by-key key-order))
      [:info no-issues-header-info-text]))

(defn- set-generic-headers! [view issues-by-key key-order]
  (ui/with-controls view [header-info-label]
    (set-label-status! header-info-label (get-header-status issues-by-key key-order))))

(defn- make-generic-controls [refresh!]
  (assert (fn? refresh!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "generic")
    (ui/children! [(doto (CheckBox. "Release mode") (.setId "release-mode-check-box") (.setFocusTraversable false) (ui/on-action! refresh!))
                   (doto (CheckBox. "Generate build report") (.setId "generate-build-report-check-box") (.setFocusTraversable false) (ui/on-action! refresh!))
                   (doto (CheckBox. "Publish Live Update content") (.setId "publish-live-update-content-check-box") (.setFocusTraversable false) (ui/on-action! refresh!))])))

(defn- read-generic-options [prefs]
  {:release-mode? (prefs/get-prefs prefs "bundle-release-mode?" true)
   :generate-build-report? (prefs/get-prefs prefs "bundle-generate-build-report?" false)
   :publish-live-update-content? (prefs/get-prefs prefs "bundle-publish-live-update-content?" false)})

(defn- write-generic-options! [prefs options]
  (when (contains? options :release-mode?)
    (prefs/set-prefs prefs "bundle-release-mode?" (:release-mode? options)))
  (when (contains? options :generate-build-report?)
    (prefs/set-prefs prefs "bundle-generate-build-report?" (:generate-build-report? options)))
  (when (contains? options :publish-live-update-content?)
    (prefs/set-prefs prefs "bundle-publish-live-update-content?" (:publish-live-update-content? options))))

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

(deftype GenericBundleOptionsPresenter [view title platform]
  BundleOptionsPresenter
  (make-views [this _owner-window]
    (assert (string? (not-empty platform)))
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers title)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge {:platform platform}
           (get-generic-options view)))
  (set-options! [_this options]
    (set-generic-options! view options)))

;; -----------------------------------------------------------------------------
;; Selectable platform
;; -----------------------------------------------------------------------------

(defn- make-platform-controls [refresh! platform-choices]
  (assert (fn? refresh!))
  (assert (sequential? (not-empty platform-choices)))
  (assert (every? (fn [[k v]] (and (string? (not-empty k)) (string? (not-empty v)))) platform-choices))
  (let [platform-values-by-label (into {} platform-choices)
        platform-labels-by-value (set/map-invert platform-values-by-label)
        platform-choice-box (doto (ChoiceBox. (ui/observable-list (map second platform-choices)))
                                  (.setId "platform-choice-box")
                                  (ui/on-action! refresh!)
                                  (.setConverter (proxy [StringConverter] []
                                                   (toString [value]
                                                     (platform-labels-by-value value))
                                                   (fromString [label]
                                                     (platform-values-by-label label)))))]
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "platform")
      (ui/children! [(labeled! "Architecture" platform-choice-box)]))))

(declare bundle-options-presenter)

(defn- get-bundle-platform-keys [platform]
  (assert (keyword? platform))
  (assert (not= :default platform))
  (assert (some? (get-method bundle-options-presenter platform)))
  (let [platform-name (name platform)
        prefs-key (str "bundle-" platform-name "-platform")
        options-key (keyword (str platform-name "-platform"))]
    [prefs-key options-key]))

(defn- read-platform-options [prefs platform default]
  (let [[prefs-key options-key] (get-bundle-platform-keys platform)
        value (prefs/get-prefs prefs prefs-key default)]
    (hash-map options-key value)))

(defn- write-platform-options! [prefs options platform]
  (let [[prefs-key options-key] (get-bundle-platform-keys platform)]
    (when (contains? options options-key)
      (prefs/set-prefs prefs prefs-key (get options options-key)))))

(defn- get-platform-options [view platform]
  (ui/with-controls view [platform-choice-box]
    (let [[_prefs-key options-key] (get-bundle-platform-keys platform)
          value (ui/value platform-choice-box)]
      (hash-map :platform value
                options-key value))))

(defn- set-platform-options! [view options platform]
  (ui/with-controls view [platform-choice-box]
    (let [[_prefs-key options-key] (get-bundle-platform-keys platform)
          value (get options options-key)]
      (ui/value! platform-choice-box value))))

(deftype SelectablePlatformBundleOptionsPresenter [view title platform platform-choices]
  BundleOptionsPresenter
  (make-views [this _owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers title)
       (make-platform-controls refresh! platform-choices)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge (get-generic-options view)
           (get-platform-options view platform)))
  (set-options! [_this options]
    (set-generic-options! view options)
    (set-platform-options! view options platform)))

;; -----------------------------------------------------------------------------
;; Android
;; -----------------------------------------------------------------------------

(defn- make-android-controls [refresh! owner-window]
  (assert (fn? refresh!))
  (let [make-file-field (partial make-file-field refresh! owner-window)
        certificate-file-field (make-file-field "certificate-text-field" "Choose Certificate" [["Certificates (*.pem)" "*.pem"]])
        private-key-file-field (make-file-field "private-key-text-field" "Choose Private Key" [["Private Keys (*.pk8)" "*.pk8"]])]
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "android")
      (ui/children! [(labeled! "Certificate" certificate-file-field)
                     (labeled! "Private key" private-key-file-field)]))))

(defn- read-android-options [prefs]
  {:certificate (get-file-pref prefs "bundle-android-certificate")
   :private-key (get-file-pref prefs "bundle-android-private-key")})

(defn- write-android-options! [prefs options]
  (when (contains? options :certificate)
    (set-file-pref! prefs "bundle-android-certificate" (:certificate options)))
  (when (contains? options :private-key)
    (set-file-pref! prefs "bundle-android-private-key" (:private-key options))))

(defn- get-android-options [view]
  (ui/with-controls view [certificate-text-field private-key-text-field]
    {:certificate (get-file certificate-text-field)
     :private-key (get-file private-key-text-field)}))

(defn- set-android-options! [view {:keys [certificate private-key] :as _options} issues]
  (ui/with-controls view [certificate-text-field private-key-text-field ok-button]
    (doto certificate-text-field
      (set-file! certificate)
      (set-field-status! (:certificate issues)))
    (doto private-key-text-field
      (set-file! private-key)
      (set-field-status! (:private-key issues)))
    (ui/enable! ok-button (or (and (nil? certificate)
                                   (nil? private-key))
                              (and (existing-file-of-type? "pem" certificate)
                                   (existing-file-of-type? "pk8" private-key))))))

(defn- get-android-issues [{:keys [certificate private-key] :as _options}]
  {:general (when (and (nil? certificate) (nil? private-key))
              [:info "Set certificate and private key, or leave blank to sign APK with an auto-generated debug certificate."])
   :certificate (cond
                  (and (some? certificate) (not (existing-file? certificate)))
                  [:fatal "Certificate file not found."]

                  (and (some? certificate) (not (existing-file-of-type? "pem" certificate)))
                  [:fatal "Invalid certificate."]

                  (and (nil? certificate) (some? private-key))
                  [:fatal "Certificate must be set if private key is specified."])
   :private-key (cond
                  (and (some? private-key) (not (existing-file? private-key)))
                  [:fatal "Private key file not found."]

                  (and (some? private-key) (not (existing-file-of-type? "pk8" private-key)))
                  [:fatal "Invalid private key."]

                  (and (some? certificate) (nil? private-key))
                  [:fatal "Private key must be set if certificate is specified."])})

(deftype AndroidBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Bundle Android Application")
       (make-android-controls refresh! owner-window)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge {:platform "armv7-android"}
           (get-generic-options view)
           (get-android-options view)))
  (set-options! [_this options]
    (let [issues (get-android-issues options)]
      (set-generic-options! view options)
      (set-android-options! view options issues)
      (set-generic-headers! view issues [:certificate :private-key]))))

;; -----------------------------------------------------------------------------
;; iOS
;; -----------------------------------------------------------------------------

(defn- get-code-signing-identity-names []
  (mapv second (bundle/find-identities)))

(defn- make-ios-controls [refresh! owner-window]
  (assert (fn? refresh!))
  (let [provisioning-profile-file-field (make-file-field refresh! owner-window "provisioning-profile-text-field" "Choose Provisioning Profile" [["Provisioning Profiles (*.mobileprovision)" "*.mobileprovision"]])
        no-identity-label "None"
        code-signing-identity-choice-box (doto (ChoiceBox.)
                                           (.setId "code-signing-identity-choice-box")
                                           (.setFocusTraversable false)
                                           (.setMaxWidth Double/MAX_VALUE) ; Required to fill available space.
                                           (.setConverter (proxy [StringConverter] []
                                                            (toString [value]
                                                              (if (some? value) value no-identity-label))
                                                            (fromString [label]
                                                              (if (= no-identity-label label) nil label)))))]
    (ui/on-action! code-signing-identity-choice-box refresh!)
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "ios")
      (ui/children! [(labeled! "Code signing identity" code-signing-identity-choice-box)
                     (labeled! "Provisioning profile" provisioning-profile-file-field)]))))


(defn- read-ios-options [prefs]
  ;; This falls back on settings from the Sign iOS Application dialog if available,
  ;; but we will write to our own keys in the preference for these.
  (let [code-signing-identity-names (get-code-signing-identity-names)]
    {:code-signing-identity (or (some (set code-signing-identity-names)
                                  (or (get-string-pref prefs "bundle-ios-code-signing-identity")
                                      (second (prefs/get-prefs prefs "last-identity" [nil nil]))))
                                (first code-signing-identity-names))
     :provisioning-profile (or (get-file-pref prefs "bundle-ios-provisioning-profile")
                               (get-file-pref prefs "last-provisioning-profile"))}))

(defn- write-ios-options! [prefs options]
  (when (contains? options :code-signing-identity)
    (set-string-pref! prefs "bundle-ios-code-signing-identity" (:code-signing-identity options)))
  (when (contains? options :provisioning-profile)
    (set-file-pref! prefs "bundle-ios-provisioning-profile" (:provisioning-profile options))))

(defn- get-ios-options [view]
  (ui/with-controls view [code-signing-identity-choice-box provisioning-profile-text-field]
    {:code-signing-identity (ui/value code-signing-identity-choice-box)
     :provisioning-profile (get-file provisioning-profile-text-field)}))

(defn- set-ios-options! [view {:keys [code-signing-identity provisioning-profile] :as _options} issues code-signing-identity-names]
  (ui/with-controls view [^ChoiceBox code-signing-identity-choice-box provisioning-profile-text-field ok-button]
    (doto code-signing-identity-choice-box
      (set-choice! (into [nil] code-signing-identity-names) code-signing-identity)
      (set-field-status! (:code-signing-identity issues))
      (ui/disable! (empty? code-signing-identity-names)))
    (doto provisioning-profile-text-field
      (set-file! provisioning-profile)
      (set-field-status! (:provisioning-profile issues)))
    (ui/enable! ok-button (and (some? code-signing-identity)
                               (existing-file? provisioning-profile)))))

(defn- get-ios-issues [{:keys [code-signing-identity provisioning-profile] :as _options} code-signing-identity-names]
  {:general (when (empty? code-signing-identity-names)
              [:fatal "No code signing identities found on this computer."])
   :code-signing-identity (when (nil? code-signing-identity)
                            [:fatal "Code signing identity must be set."])
   :provisioning-profile (cond
                           (nil? provisioning-profile)
                           [:fatal "Provisioning profile must be set."]

                           (not (existing-file? provisioning-profile))
                           [:fatal "Provisioning profile file not found."]

                           (not (existing-file-of-type? "mobileprovision" provisioning-profile))
                           [:fatal "Invalid provisioning profile."])})

(deftype IOSBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this owner-window]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Bundle iOS Application")
       (make-ios-controls refresh! owner-window)
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (merge {:platform "armv7-darwin"}
           (get-generic-options view)
           (get-ios-options view)))
  (set-options! [_this options]
    (let [code-signing-identity-names (get-code-signing-identity-names)
          issues (get-ios-issues options code-signing-identity-names)]
      (set-generic-options! view options)
      (set-ios-options! view options issues code-signing-identity-names)
      (set-generic-headers! view issues [:code-signing-identity :provisioning-profile]))))

;; -----------------------------------------------------------------------------

(defmulti bundle-options-presenter (fn [_view platform] platform))
(defmethod bundle-options-presenter :default [_view platform] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-options-presenter :android [view _platform] (AndroidBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :html5   [view _platform] (GenericBundleOptionsPresenter. view "Bundle HTML5 Application" "js-web"))
(defmethod bundle-options-presenter :ios     [view _platform] (IOSBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :linux   [view _platform] (SelectablePlatformBundleOptionsPresenter. view "Bundle Linux Application" :linux [["32-bit" "x86-linux"]
                                                                                                                                                 ["64-bit" "x86_64-linux"]]))
(defmethod bundle-options-presenter :macos   [view _platform] (GenericBundleOptionsPresenter. view "Bundle macOS Application" "x86-darwin")) ; TODO: The minimum OS X version we run on is 10.7 Lion, which does not support 32-bit processors. Shouldn't this be "x86_64-darwin"?
(defmethod bundle-options-presenter :windows [view _platform] (SelectablePlatformBundleOptionsPresenter. view "Bundle Windows Application" :windows [["32-bit" "x86-win32"]
                                                                                                                                                     ["64-bit" "x86_64-win32"]]))

(defn- read-build-options [prefs]
  (merge (read-generic-options prefs)
         (read-android-options prefs)
         (read-ios-options prefs)
         (read-platform-options prefs :linux (if (= (system/os-arch) "x86") "x86-linux" "x86_64-linux"))
         (read-platform-options prefs :windows (if (= (system/os-arch) "x86") "x86-win32" "x86_64-win32"))))

(defn- write-build-options! [prefs build-options]
  (write-generic-options! prefs build-options)
  (write-android-options! prefs build-options)
  (write-ios-options! prefs build-options)
  (write-platform-options! prefs build-options :linux)
  (write-platform-options! prefs build-options :windows))

(defn- watch-focus-state! [presenter]
  (assert (satisfies? BundleOptionsPresenter presenter))
  (add-watch dialogs/focus-state ::focus-state-watch
             (fn [_key _ref _old _new]
               (refresh-presenter! presenter))))

(defn- unwatch-focus-state! [_event]
  (remove-watch dialogs/focus-state ::focus-state-watch))

(handler/defhandler ::close :bundle-dialog
  (run [stage]
    (ui/close! stage)))

(handler/defhandler ::query-output-directory :bundle-dialog
  (run [bundle! prefs presenter stage]
    (let [build-options (get-options presenter)
          initial-directory (get-file-pref prefs "bundle-output-directory")]
      (assert (string? (not-empty (:platform build-options))))
      (write-build-options! prefs build-options)
      (when-let [output-directory (query-directory! "Output Directory" initial-directory stage)]
        (set-file-pref! prefs "bundle-output-directory" output-directory)
        (let [platform-bundle-output-directory (io/file output-directory (:platform build-options))
              platform-bundle-output-directory-exists? (.exists platform-bundle-output-directory)]
          (when (or (not platform-bundle-output-directory-exists?)
                    (query-overwrite! platform-bundle-output-directory stage))
            (when (not platform-bundle-output-directory-exists?)
              (.mkdirs platform-bundle-output-directory))
            (let [build-options (assoc build-options :output-directory platform-bundle-output-directory)]
              (ui/close! stage)
              (bundle! build-options))))))))

(defn show-bundle-dialog! [platform prefs owner-window bundle!]
  (let [build-options (read-build-options prefs)
        stage (doto (ui/make-dialog-stage owner-window)
                (ui/title! "Bundle Application")
                (dialogs/observe-focus))
        root (doto (VBox.)
               (ui/add-style! "bundle-dialog")
               (ui/add-style! (name platform))
               (ui/apply-css!))
        presenter (bundle-options-presenter root platform)]

    ;; Dialog context.
    (ui/context! root :bundle-dialog {:bundle! bundle!
                                      :prefs prefs
                                      :presenter presenter
                                      :stage stage} nil)
    ;; Key bindings.
    (ui/ensure-receives-key-events! stage)
    (ui/bind-keys! root {KeyCode/ESCAPE ::close})

    ;; Presenter views.
    (doseq [view (make-views presenter stage)]
      (ui/add-child! root view))

    ;; Button bar.
    (let [ok-button (doto (Button. "Create Bundle...") (.setId "ok-button") (.setFocusTraversable false))
          buttons (doto (HBox.) (ui/add-style! "buttons"))]
      (ui/add-child! buttons ok-button)
      (ui/add-child! root buttons)
      (ui/bind-action! ok-button ::query-output-directory))

    ;; Refresh from build options.
    (set-options! presenter build-options)

    ;; Refresh whenever our application becomes active.
    (watch-focus-state! presenter)
    (ui/on-closed! stage unwatch-focus-state!)

    ;; Show dialog.
    (let [scene (Scene. root)]
      (.setScene stage scene)
      (ui/show! stage))))
