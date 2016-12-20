(ns editor.bundle
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs]
            [editor.system :as system]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.engine :as engine])
  (:import [java.io File]
           [javafx.scene Parent Node Scene Group]
           [javafx.stage Stage StageStyle Modality]
           [com.google.common.io Files]
           [com.defold.editor Platform]
           [org.apache.commons.configuration.plist XMLPropertyListConfiguration]))

(set! *warn-on-reflection* true)

(defn- get-ios-engine [workspace prefs]
  (let [armv7 ^File (engine/get-engine workspace prefs "armv7-ios")
        arm64 ^File (engine/get-engine workspace prefs "arm64-ios")
        unpack (System/getProperty "defold.unpack.path")
        lipo (format "%s/%s/bin/lipo" unpack (.getPair (Platform/getJavaPlatform)))
        engine (File/createTempFile "dmengine" "")]
    (shell/sh lipo "-create" (.getAbsolutePath armv7) (.getAbsolutePath arm64) "-output" (.getAbsolutePath engine))
    engine))

(defn- extract-entitlement [profile]
  (let [text-profile (File/createTempFile "mobileprovision" ".plist")]
    (shell/sh "security"  "cms"  "-D"  "-i"  profile "-o"  (.getAbsolutePath text-profile))

    (let [profile-info (XMLPropertyListConfiguration.)
          entitlements-info (XMLPropertyListConfiguration.)
          entitlements (File/createTempFile "entitlement" ".xcent")]
      (with-open [r (io/reader text-profile)]
        (.load profile-info r))
      (.append entitlements-info (.configurationAt profile-info "Entitlements"))
      (.save entitlements-info entitlements)
      (.getAbsolutePath entitlements))))

(defn- sign-ios-app [ipa exe identity profile props]
  (let [unpack (System/getProperty "defold.unpack.path")
        codesign (format "%s/bin/codesign" unpack)
        codesign-alloc (format "%s/bin/codesign_allocate" unpack)
        package-dir (Files/createTempDir)
        payload-dir (io/file package-dir "Payload")
        app-dir (io/file payload-dir "Defold.app")
        info (XMLPropertyListConfiguration.)]
    (.mkdirs app-dir)
    (with-open [r (io/reader (io/resource "bundle/ios/Info-dev-app.plist"))]
      (.load info r))
    (doseq [[k v]  props]
      (.setProperty info k v))
    (.save info (io/file app-dir "Info.plist"))

    ;; copy icons
    (doseq [icon ["ios_icon_57.png", "ios_icon_114.png", "ios_icon_72.png", "ios_icon_144.png"]]
      (io/copy (slurp (io/resource (str "icons/ios/" icon))) (io/file app-dir icon)))

    (io/copy (io/file  profile) (io/file app-dir "embedded.mobileprovision"))

    (io/copy (io/file exe) (io/file app-dir "dmengine"))

    (let [entitlements (extract-entitlement profile)
          env {"EMBEDDED_PROFILE_NAME" "embedded.mobileprovision"
               "CODESIGN_ALLOCATE" codesign-alloc}]
      (shell/sh "codesign" "-f" "-s" identity "--entitlements" entitlements (.getAbsolutePath app-dir) :env env))

    (.delete (io/file ipa))
    (shell/sh "zip" "-qr" ipa "Payload" :dir package-dir)
    app-dir))

(handler/defhandler ::sign :dialog
  (enabled? [controls] (and (ui/selection (:identities controls))
                            (.exists (io/file (ui/text (:provisioning-profile controls))))))
  (run [workspace prefs ^Stage stage root controls project]


    (let [ipa-dir (ui/choose-directory "Select target directory for IPA file")]
      (when ipa-dir
        (let [settings (g/node-value project :settings)
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

              ^File engine (get-ios-engine workspace prefs)
              identity (get (ui/selection (:identities controls)) 0)
              identity-id (get identity 0)
              profile (ui/text (:provisioning-profile controls))]

          (prefs/set-prefs prefs "last-identity" identity)
          (prefs/set-prefs prefs "last-provisioning-profile" profile)
          (ui/disable! root true)
          (sign-ios-app (format "%s/%s.ipa" ipa-dir name) (.getAbsolutePath engine) identity-id profile props))))
    (.close stage)))

(handler/defhandler ::select-provisioning-profile :dialog
  (enabled? [] true)
  (run [stage controls]
    (let [f (ui/choose-file "Selection Provisioning Profile" "Provisioning Profile" ["*.mobileprovision"])]
      (ui/text! (:provisioning-profile controls) f))))

(defn- find-identities []
  (let [re #"\s+\d+\)\s+([0-9A-Z]+)\s+\"(.*?)\""
        lines (.split ^String (:out (shell/sh "security" "find-identity" "-v" "-p" "codesigning")) "\n" )]
    (->> lines
         (map #(first (re-seq re %)))
         (remove nil?)
         (map (fn [[_ id name]] [id name])))))

(defn make-sign-dialog [workspace prefs project]
  (let [root ^Parent (ui/load-fxml "sign-dialog.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["identities" "sign" "provisioning-profile" "provisioning-profile-button"])
        identities (find-identities)]

    (ui/context! root :dialog {:root root :workspace workspace :prefs prefs :controls controls :stage stage :project project} nil)
    (ui/cell-factory! (:identities controls) (fn [i] {:text (second i)}))

    (ui/text! (:provisioning-profile controls) (prefs/get-prefs prefs "last-provisioning-profile" ""))

    (ui/bind-action! (:provisioning-profile-button controls) ::select-provisioning-profile)
    (ui/bind-action! (:sign controls) ::sign)

    (ui/items! (:identities controls) identities)

    (let [last-identity (prefs/get-prefs prefs "last-identity" "")]
      (when (some #(= % last-identity) identities)
        (ui/select! (:identities controls) last-identity)))

    (dialogs/observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Sign iOS Application")
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))
