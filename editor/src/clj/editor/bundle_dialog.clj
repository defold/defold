(ns editor.bundle-dialog
  (:require [editor.ui :as ui])
  (:import (java.io File)
           (javafx.scene Scene)
           (javafx.scene.control Button CheckBox Label)
           (javafx.scene.layout AnchorPane HBox VBox)
           (javafx.stage DirectoryChooser Window)))

(set! *warn-on-reflection* true)

(defn- query-output-directory!
  ^File [^Window owner ^File initial-directory]
  (let [chooser (DirectoryChooser.)]
    (when (and (some? initial-directory) (.exists initial-directory))
      (.setInitialDirectory initial-directory))
    (.setTitle chooser "Output Directory")
    (.showDialog chooser owner)))

(defprotocol BundleOptionsPresenter
  (make-views [this commit!])
  (get-state [this])
  (set-state! [this state]))

(defn- make-generic-headers [header description]
  (doto (AnchorPane.)
    (ui/add-style! "headers")
    (ui/children! [(doto (Label. header) (ui/add-style! "header-label"))
                   (doto (Label. description) (ui/add-style! "description-label"))])))

(defn- set-generic-headers-description! [view description]
  (ui/with-controls view [^Label description-label]
    (.setText description-label description)))

(defn- make-generic-controls [commit!]
  (assert (fn? commit!))
  (let [on-action! (fn on-action! [_] (commit!))
        commit-on-action! (fn commit-on-action! [control] (ui/on-action! control on-action!))]
    (doto (VBox.)
      (ui/add-style! "settings")
      (ui/add-style! "generic")
      (ui/children! [(doto (CheckBox. "Release mode") (.setId "release-mode-checkbox") (commit-on-action!))
                     (doto (CheckBox. "Generate build report") (.setId "generate-build-report-checkbox") (commit-on-action!))
                     (doto (CheckBox. "Publish Live Update content") (.setId "publish-live-update-content-checkbox") (commit-on-action!))]))))

(defn- make-generic-state []
  {:release-mode? true
   :generate-build-report? false
   :publish-live-update-content? false})

(defn- get-generic-state [view]
  (ui/with-controls view [^CheckBox release-mode-checkbox
                          ^CheckBox generate-build-report-checkbox
                          ^CheckBox publish-live-update-content-checkbox]
    {:release-mode? (.isSelected release-mode-checkbox)
     :generate-build-report? (.isSelected generate-build-report-checkbox)
     :publish-live-update-content? (.isSelected publish-live-update-content-checkbox)}))

(defn- set-generic-state! [view state]
  (ui/with-controls view [^CheckBox release-mode-checkbox
                          ^CheckBox generate-build-report-checkbox
                          ^CheckBox publish-live-update-content-checkbox]
    (.setSelected release-mode-checkbox (:release-mode? state))
    (.setSelected generate-build-report-checkbox (:generate-build-report? state))
    (.setSelected publish-live-update-content-checkbox (:publish-live-update-content? state))))

(deftype GenericBundleOptionsPresenter [view title]
  BundleOptionsPresenter
  (make-views [_this commit!]
    [(make-generic-headers title "Set build options")
     (make-generic-controls commit!)])
  (get-state [_this]
    (get-generic-state view))
  (set-state! [_this state]
    (set-generic-state! view state)))

(defn- make-android-controls [commit!]
  (assert (fn? commit!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "android")
    (ui/children! [(Label. "Android Settings...")])))

(defn- make-android-state []
  {})

(defn- get-android-state [view]
  {})

(defn- set-android-state! [view state]
  )

(deftype AndroidBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [_this commit!]
    [(make-generic-headers "Package Android Application" "Set certificate and private key (leave fields blank to sign APK with an auto-generated debug certificate)")
     (make-android-controls commit!)
     (make-generic-controls commit!)])
  (get-state [_this]
    (merge (get-generic-state view)
           (get-android-state view)))
  (set-state! [_this state]
    (set-generic-state! view state)
    (set-android-state! view state)))

(defn- make-ios-controls [commit!]
  (assert (fn? commit!))
  (doto (VBox.)
    (ui/add-style! "settings")
    (ui/add-style! "ios")
    (ui/children! [(Label. "iOS Settings...")])))

(defn- make-ios-state []
  {})

(defn- get-ios-state [view]
  {})

(defn- set-ios-state! [view state]
  )

(deftype IOSBundleOptionsPresenter [view]
  BundleOptionsPresenter
  (make-views [_this commit!]
    [(make-generic-headers "Package iOS Application" "Set build options")
     (make-ios-controls commit!)
     (make-generic-controls commit!)])
  (get-state [_this]
    (merge (get-generic-state view)
           (get-ios-state view)))
  (set-state! [_this state]
    (set-generic-state! view state)
    (set-ios-state! view state)))

(defmulti bundle-options-presenter (fn [_view platform] platform))
(defmethod bundle-options-presenter :default [_view platform] (throw (IllegalArgumentException. (str "Unsupported platform: " platform))))
(defmethod bundle-options-presenter :android [view _platform] (AndroidBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :html5   [view _platform] (GenericBundleOptionsPresenter. view "Package HTML5 Application"))
(defmethod bundle-options-presenter :ios     [view _platform] (IOSBundleOptionsPresenter. view))
(defmethod bundle-options-presenter :linux   [view _platform] (GenericBundleOptionsPresenter. view "Package Linux Application"))
(defmethod bundle-options-presenter :macos   [view _platform] (GenericBundleOptionsPresenter. view "Package macOS Application"))
(defmethod bundle-options-presenter :windows [view _platform] (GenericBundleOptionsPresenter. view "Package Windows Application"))

(defn make-bundle-dialog-state []
  (merge (make-generic-state)
         (make-android-state)
         (make-ios-state)))

(defn show-bundle-dialog! [dialog-state-atom platform ^Window owner]
  (let [stage (doto (ui/make-dialog-stage owner) (ui/title! "Package Application"))
        root (doto (VBox.) (ui/add-style! "bundle-view") (ui/apply-css!))
        presenter (bundle-options-presenter root platform)
        update-presenter-from-state! (let [update-in-progress (volatile! false)]
                                       (fn []
                                         (when-not @update-in-progress
                                           (vreset! update-in-progress true)
                                           (try
                                             (set-state! presenter @dialog-state-atom)
                                             (finally
                                               (vreset! update-in-progress false))))))
        update-state-from-presenter! (fn []
                                       (reset! dialog-state-atom (get-state presenter)))
        commit! (fn []
                  (update-state-from-presenter!)
                  (update-presenter-from-state!))]

    ;; Presenter views.
    (doseq [view (make-views presenter commit!)]
      (ui/add-child! root view))

    (update-presenter-from-state!)

    ;; Button bar.
    (let [ok-button (doto (Button. "Package"))
          buttons (doto (HBox.) (ui/add-style! "buttons"))]
      (ui/add-child! root buttons)
      (ui/on-action! ok-button (fn on-ok! [_]
                                 (ui/close! stage)
                                 (if-let [output-directory (query-output-directory! owner (:output-directory @dialog-state-atom))]
                                   (do (swap! dialog-state-atom assoc :output-directory output-directory)
                                       (println "Bundle to" (:output-directory @dialog-state-atom)))
                                   (show-bundle-dialog! dialog-state-atom platform owner)))))

    ;; Show dialog.
    (let [scene (Scene. root)]
      (.setScene stage scene)
      (ui/show! stage))))
