(ns editor.bundle
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.client :as client]
            [editor.fs :as fs]
            [editor.login :as login]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.engine :as engine]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.git :as git]
            [editor.workspace :as workspace])
  (:import [clojure.lang ExceptionInfo]
           [java.io File]
           [java.net URL]
           [javafx.scene Parent Scene]
           [javafx.stage Stage Modality]
           [com.defold.editor Platform]
           [org.apache.commons.configuration.plist XMLPropertyListConfiguration]))

(set! *warn-on-reflection* true)

(defn- get-ios-engine [project prefs]
  (let [armv7 ^File (engine/get-engine project prefs "armv7-darwin")
        arm64 ^File (engine/get-engine project prefs "arm64-darwin")
        unpack (System/getProperty "defold.unpack.path")
        lipo (format "%s/%s/bin/lipo" unpack (.getPair (Platform/getJavaPlatform)))
        engine (fs/create-temp-file! "dmengine" "")]
    (shell/sh lipo "-create" (.getAbsolutePath armv7) (.getAbsolutePath arm64) "-output" (.getAbsolutePath engine))
    engine))

(defn- extract-entitlement [profile]
  (let [text-profile (fs/create-temp-file! "mobileprovision" ".plist")]
    (shell/sh "security"  "cms"  "-D"  "-i"  profile "-o"  (.getAbsolutePath text-profile))

    (let [profile-info (XMLPropertyListConfiguration.)
          entitlements-info (XMLPropertyListConfiguration.)
          entitlements (fs/create-temp-file! "entitlement" ".xcent")]
      (with-open [r (io/reader text-profile)]
        (.load profile-info r))
      (.append entitlements-info (.configurationAt profile-info "Entitlements"))
      (.save entitlements-info entitlements)
      (.getAbsolutePath entitlements))))

(defn- sign-ios-app [ipa exe identity profile props]
  (let [unpack (System/getProperty "defold.unpack.path")
        codesign (format "%s/bin/codesign" unpack)
        codesign-alloc (format "%s/bin/codesign_allocate" unpack)
        package-dir (fs/create-temp-directory!)
        payload-dir (io/file package-dir "Payload")
        app-dir (io/file payload-dir "Defold.app")
        info (XMLPropertyListConfiguration.)]
    (fs/create-directories! app-dir)
    (with-open [r (io/reader (io/resource "bundle/ios/Info-dev-app.plist"))]
      (.load info r))
    (doseq [[k v]  props]
      (.setProperty info k v))
    (.save info (io/file app-dir "Info.plist"))

    ;; copy icons
    (doseq [icon ["ios_icon_57.png", "ios_icon_114.png", "ios_icon_72.png", "ios_icon_144.png"]]
      (with-open [icon-stream (io/input-stream (io/resource (str "icons/ios/" icon)))]
        (io/copy icon-stream (io/file app-dir icon))))

    (fs/copy-file! (io/file profile) (io/file app-dir "embedded.mobileprovision"))
    (fs/copy-file! (io/file exe) (io/file app-dir "dmengine"))

    (let [entitlements (extract-entitlement profile)
          env {"EMBEDDED_PROFILE_NAME" "embedded.mobileprovision"
               "CODESIGN_ALLOCATE" codesign-alloc}]
      (shell/sh "codesign" "-f" "-s" identity "--entitlements" entitlements (.getAbsolutePath app-dir) :env env))

    (fs/delete-file! (io/file ipa))

    (shell/sh "zip" "-qr" ipa "Payload" :dir package-dir)
    app-dir))

(defn- cr-project-id [workspace]
  (when-let [git (git/open (workspace/project-path workspace))]
    (try
      (when-let [remote-url (git/remote-origin-url git)]
        (let [url (URL. remote-url)
              host (.getHost url)]
          (when (or (= host "cr.defold.se") (= host "cr.defold.com"))
            (let [path (.getPath url)]
              (try
                (Long/parseLong (subs path (inc (string/last-index-of path "/"))))
                (catch NumberFormatException e
                  nil))))))
      (finally
        (.close git)))))

(handler/defhandler ::sign :dialog
  (enabled? [controls] (and (ui/selection (:identities controls))
                            (.exists (io/file (ui/text (:provisioning-profile controls))))
                            (.isDirectory (io/file (ui/text (:build-dir controls))))))
  (run [workspace prefs ^Stage stage root controls project build-options]
    (let [clear-errors! (:clear-errors! build-options)
          render-error! (:render-error! build-options)
          ^File engine (try
                         (clear-errors!)
                         (get-ios-engine project prefs)
                         (catch Exception e
                           (when-not (engine-build-errors/handle-build-error! render-error! project e)
                             (throw e))
                           nil))]
      (if (nil? engine)
        (do (ui/close! stage)
            (dialogs/make-alert-dialog "Failed to build ipa with Native Extensions. Please fix build errors and try again."))
        (let [ipa-dir (ui/text (:build-dir controls))
              settings (g/node-value project :settings)
              w (get settings ["display" "width"] 1)
              h (get settings ["display" "height"] 1)
              orient-props (if (> w h)
                             {"UISupportedInterfaceOrientations"      "UIInterfaceOrientationLandscapeRight"
                              "UISupportedInterfaceOrientations~ipad" "UIInterfaceOrientationLandscapeRight"}
                             {"UISupportedInterfaceOrientations"      "UIInterfaceOrientationPortrait"
                              "UISupportedInterfaceOrientations~ipad" "UIInterfaceOrientationPortrait"})
              name (get settings ["project" "title"] "Unnamed")
              props {"CFBundleDisplayName" name
                     "CFBundleExecutable" "dmengine"
                     "CFBundleIdentifier" (get settings ["ios" "bundle_identifier"] "dmengine")}

              identity (get (ui/selection (:identities controls)) 0)
              identity-id (get identity 0)
              profile (ui/text (:provisioning-profile controls))]

          (prefs/set-prefs prefs "last-identity" identity)
          (prefs/set-prefs prefs "last-provisioning-profile" profile)
          (prefs/set-prefs prefs "last-ios-build-dir" ipa-dir)
          (let [ipa (format "%s/%s.ipa" ipa-dir name)
                cr-project-id (cr-project-id workspace)]
            (when (or (nil? cr-project-id)
                      (login/login prefs))
              (ui/disable! root true)
              (sign-ios-app ipa (.getAbsolutePath engine) identity-id profile props)
              (when (some? cr-project-id)
                (let [client (client/make-client prefs)]
                  (when-let [user-id (client/user-id client)]
                    (with-open [in (io/input-stream ipa)]
                      (try
                        (client/upload-engine client user-id cr-project-id "ios" in)
                        (ui/run-later
                          (dialogs/make-alert-dialog "Successfully uploaded a signed ipa to the project dashboard. Team members can download it to their device from the Settings page."))
                        (catch ExceptionInfo e
                          (if (= 403 (:status (ex-data e)))
                            (ui/run-later
                              (dialogs/make-alert-dialog "You are not authorized to upload a signed ipa to the project dashboard."))
                            (throw e))))))))
              (ui/close! stage))))))))

(handler/defhandler ::select-provisioning-profile :dialog
  (enabled? [] true)
  (run [stage controls]
    (let [f (ui/choose-file "Select Provisioning Profile" "Provisioning Profile (*.mobileprovision)" ["*.mobileprovision"])]
      (ui/text! (:provisioning-profile controls) f))))

(handler/defhandler ::select-build-dir :dialog
  (enabled? [] true)
  (run [stage controls]
    (let [^File f (io/as-file (ui/text (:build-dir controls)))
          f (when (.exists f) f)]
      (when-let [f (ui/choose-directory "Select Build Directory" f)]
        (ui/text! (:build-dir controls) f)))))

(defn find-identities []
  (let [re #"\s+\d+\)\s+([0-9A-Z]+)\s+\"(.*?)\""
        lines (.split ^String (:out (shell/sh "security" "find-identity" "-v" "-p" "codesigning")) "\n" )]
    (->> lines
         (map #(first (re-seq re %)))
         (remove nil?)
         (map (fn [[_ id name]] [id name])))))

(defn make-sign-dialog [workspace prefs project build-options]
  (let [root ^Parent (ui/load-fxml "sign-dialog.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["identities" "sign" "provisioning-profile" "provisioning-profile-button" "build-dir" "build-dir-button"])
        identities (find-identities)]

    (ui/context! root :dialog {:root root :workspace workspace :prefs prefs :controls controls :stage stage :project project :build-options build-options} nil)
    (ui/cell-factory! (:identities controls) (fn [i] {:text (second i)}))

    (ui/text! (:provisioning-profile controls) (prefs/get-prefs prefs "last-provisioning-profile" ""))
    (ui/text! (:build-dir controls) (prefs/get-prefs prefs "last-ios-build-dir" (str (workspace/project-path workspace) "/build")))

    (ui/bind-action! (:provisioning-profile-button controls) ::select-provisioning-profile)
    (ui/bind-action! (:build-dir-button controls) ::select-build-dir)
    (ui/bind-action! (:sign controls) ::sign)

    (ui/items! (:identities controls) identities)

    (let [last-identity (prefs/get-prefs prefs "last-identity" "")]
      (when (some #(= % last-identity) identities)
        (ui/select! (:identities controls) last-identity)))

    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Sign iOS Application")
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))
