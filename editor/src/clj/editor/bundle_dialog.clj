(ns editor.bundle-dialog
  (:require [editor.ui :as ui])
  (:import (java.io File)
           (javafx.scene Scene)
           (javafx.scene.control Button CheckBox Label)
           (javafx.scene.layout HBox VBox)
           (javafx.stage DirectoryChooser Window)))

(set! *warn-on-reflection* true)

(defn- query-output-directory!
  ^File [^Window owner ^File initial-directory]
  (let [chooser (DirectoryChooser.)]
    (when (and (some? initial-directory) (.exists initial-directory))
      (.setInitialDirectory chooser initial-directory))
    (.setTitle chooser "Output Directory")
    (.showDialog chooser owner)))

(defprotocol BundleOptionsPresenter
  (make-views [this])
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
;; All platforms
;; -----------------------------------------------------------------------------

(defn- make-generic-headers [header description]
  (doto (VBox.)
    (ui/add-style! "headers")
    (ui/children! [(doto (Label. header) (ui/add-style! "header-label"))
                   (doto (Label. description) (ui/add-style! "description-label"))])))

(defn- set-generic-headers-description! [view description]
  (ui/with-controls view [^Label description-label]
    (.setText description-label description)))

(defn- make-generic-controls [refresh!]
  (assert (fn? refresh!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "generic")
    (ui/children! [(doto (CheckBox. "Release mode") (.setId "release-mode-checkbox") (ui/on-action! refresh!))
                   (doto (CheckBox. "Generate build report") (.setId "generate-build-report-checkbox") (ui/on-action! refresh!))
                   (doto (CheckBox. "Publish Live Update content") (.setId "publish-live-update-content-checkbox") (ui/on-action! refresh!))])))

(def ^:private default-generic-options
  {:release-mode? true
   :generate-build-report? false
   :publish-live-update-content? false})

(defn- get-generic-options [view]
  (ui/with-controls view [^CheckBox release-mode-checkbox
                          ^CheckBox generate-build-report-checkbox
                          ^CheckBox publish-live-update-content-checkbox]
    {:release-mode? (.isSelected release-mode-checkbox)
     :generate-build-report? (.isSelected generate-build-report-checkbox)
     :publish-live-update-content? (.isSelected publish-live-update-content-checkbox)}))

(defn- set-generic-options! [view options]
  (ui/with-controls view [^CheckBox release-mode-checkbox
                          ^CheckBox generate-build-report-checkbox
                          ^CheckBox publish-live-update-content-checkbox]
    (.setSelected release-mode-checkbox (:release-mode? options))
    (.setSelected generate-build-report-checkbox (:generate-build-report? options))
    (.setSelected publish-live-update-content-checkbox (:publish-live-update-content? options))))

(deftype GenericBundleOptionsPresenter [view title]
  BundleOptionsPresenter
  (make-views [this]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers title "Set build options")
       (make-generic-controls refresh!)]))
  (get-options [_this]
    (get-generic-options view))
  (set-options! [_this options]
    (set-generic-options! view options)))

;; -----------------------------------------------------------------------------
;; Android
;; -----------------------------------------------------------------------------

(defn- make-android-controls [refresh!]
  (assert (fn? refresh!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "android")
    (ui/children! [(Label. "Android Settings...")])))

(def ^:private default-android-options
  {})

(defn- get-android-options [view]
  {})

(defn- set-android-options! [view options]
  (ui/with-controls view [^Button ok-button]
    (.setDisable ok-button true)))

(deftype AndroidBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Package Android Application" "Set certificate and private key (leave fields blank to sign APK with an auto-generated debug certificate)")
       (make-android-controls refresh!)
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

(defn- make-ios-controls [commit!]
  (assert (fn? commit!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "ios")
    (ui/children! [(Label. "iOS Settings...")])))

(def ^:private default-ios-options
  {})

(defn- get-ios-options [view]
  {})

(defn- set-ios-options! [view options]
  (ui/with-controls view [^Button ok-button]
    (.setDisable ok-button true)))

(deftype IOSBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [this]
    (let [refresh! (make-presenter-refresher this)]
      [(make-generic-headers "Package iOS Application" "Set build options")
       (make-ios-controls refresh!)
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
(defmethod bundle-options-presenter :linux   [view _platform] (GenericBundleOptionsPresenter. view "Package Linux Application"))
(defmethod bundle-options-presenter :macos   [view _platform] (GenericBundleOptionsPresenter. view "Package macOS Application"))
(defmethod bundle-options-presenter :windows [view _platform] (GenericBundleOptionsPresenter. view "Package Windows Application"))

(def ^:private default-build-options
  (merge default-generic-options
         default-android-options
         default-ios-options))

(defn show-bundle-dialog! [build-options platform owner-window]
  (let [build-options (merge default-build-options build-options)
        stage (doto (ui/make-dialog-stage owner-window) (ui/title! "Package Application"))
        root (doto (VBox.) (ui/add-style! "bundle-view") (ui/apply-css!))
        presenter (bundle-options-presenter root platform)]

    ;; Presenter views.
    (doseq [view (make-views presenter)]
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
                         (if-let [output-directory (query-output-directory! owner-window (:output-directory build-options))]
                           (let [build-options (assoc build-options :output-directory output-directory)]
                             (println "Bundle with settings" build-options))
                           (show-bundle-dialog! build-options platform owner-window))))))

    ;; Refresh from build options.
    (set-options! presenter build-options)

    ;; Show dialog.
    (let [scene (Scene. root)]
      (.setScene stage scene)
      (ui/show! stage))))
