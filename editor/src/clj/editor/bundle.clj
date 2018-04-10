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
            [editor.error-reporting :as error-reporting]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.git :as git]
            [editor.system :as system]
            [editor.workspace :as workspace])
  (:import [clojure.lang ExceptionInfo]
           [java.io File]
           [java.net URL]
           [javafx.scene Parent Scene]
           [javafx.stage Stage Modality]
           [com.defold.editor Platform]
           [org.apache.commons.configuration.plist XMLPropertyListConfiguration]))

(set! *warn-on-reflection* true)

(defn- sh [& args]
  (try
    (let [result (apply shell/sh args)]
      (if (zero? (:exit result))
        result
        (throw (ex-info (format "Shell call failed:\n%s" args) result))))
    (catch Exception e
      (throw (ex-info (format "Shell call failed:\n%s" args) {} e)))))

(defn- cr-project-id [workspace]
  (when-let [git (git/try-open (workspace/project-path workspace))]
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

(handler/defhandler ::select-provisioning-profile :dialog
  (enabled? [] true)
  (run [stage controls]
    (let [f (some-> (ui/choose-file {:title "Select Provisioning Profile"
                                     :filters [{:description "Provisioning Profile (*.mobileprovision)"
                                                :exts ["*.mobileprovision"]}]})
                    (.getAbsolutePath))]
      (ui/text! (:provisioning-profile controls) f))))

(handler/defhandler ::select-build-dir :dialog
  (enabled? [] true)
  (run [stage controls]
    (let [^File f (io/as-file (ui/text (:build-dir controls)))
          f (when (.exists f) f)]
      (when-let [f (ui/choose-directory "Select Build Directory" f)]
        (ui/text! (:build-dir controls) f)))))

;; For matching lines like:
;;   1) 0123456789ABCDEF0123456789ABCDEF01234567 "iPhone Developer: erik.angelin@king.com (0123456789)"
;;      ^-first group--------------------------^  ^-second group-------------------------------------^
(def ^:private identity-regexp #"\s+\d+\)\s+([0-9A-Z]+)\s+\"(.*?)\"")

(defn find-identities []
  (->> (.split ^String (:out (sh "security" "find-identity" "-v" "-p" "codesigning")) "\n")
       (map #(first (re-seq identity-regexp %)))
       (remove nil?)
       (map (fn [[_ id name]] [id name]))))

(g/defnk get-armv7-engine [project prefs]
  (try {:armv7 (engine/get-engine project prefs "armv7-darwin")}
       (catch Exception e
         {:err e :message "Failed to get armv7 engine."})))

(g/defnk get-arm64-engine [project prefs]
  (try {:arm64 (engine/get-engine project prefs "arm64-darwin")}
       (catch Exception e
         {:err e :message "Failed to get arm64 engine."})))

(g/defnk lipo-ios-engine [^File armv7 ^File arm64]
  (let [lipo (format "%s/%s/bin/lipo" (system/defold-unpack-path) (.getPair (Platform/getJavaPlatform)))
        engine (fs/create-temp-file! "dmengine" "")]
    (try (sh lipo "-create" (.getAbsolutePath armv7) (.getAbsolutePath arm64) "-output" (.getAbsolutePath engine))
         {:engine engine}
         (catch Exception e
           {:err e :message "Failed to lipo engine binary."}))))

(g/defnk assemble-ios-app [^File engine ^File info-plist ^File package-dir ^File app-dir ^File profile]
  (try
    ;; copy icons
    (doseq [icon ["ios_icon_57.png", "ios_icon_114.png", "ios_icon_72.png", "ios_icon_144.png"]]
      (with-open [icon-stream (io/input-stream (io/resource (str "icons/ios/" icon)))]
        (io/copy icon-stream (io/file app-dir icon))))

    (fs/copy-file! (io/file profile) (io/file app-dir "embedded.mobileprovision"))
    (fs/copy-file! engine (io/file app-dir "dmengine"))
    (fs/set-executable! engine)
    (fs/copy-file! info-plist (io/file app-dir "Info.plist"))
    {:assembled true}
    (catch Exception e
      {:err e :message "Failed to create ios app."})))

(g/defnk sign-ios-app [^File package-dir ^File app-dir ^File entitlements-plist identity-id assembled]
  (let [codesign-alloc (format "%s/%s/bin/codesign_allocate" (system/defold-unpack-path) (.getPair (Platform/getJavaPlatform)))
        codesign-env {"EMBEDDED_PROFILE_NAME" "embedded.mobileprovision"
                      "CODESIGN_ALLOCATE" codesign-alloc}]
    (try
      (sh "codesign" "-f" "-s" identity-id "--entitlements" (.getAbsolutePath entitlements-plist) (.getAbsolutePath app-dir) :env codesign-env)
      {:signed true}
      (catch Exception e
        {:err e :message "Failed to sign ios app."}))))

(g/defnk package-ipa [^File ipa-file ^File package-dir signed]
  (try
    (fs/delete-file! ipa-file)
    (sh "zip" "-qr" (.getAbsolutePath ipa-file) "Payload" :dir package-dir)
    {:ipa ipa-file}
    (catch Exception e
      {:err e :message "Failed to create ipa."})))

(g/defnk make-provisioning-profile-plist [profile]
  (try
    (let [profile-plist (fs/create-temp-file! "mobileprovision" ".plist")]
      (sh "security"  "cms"  "-D"  "-i"  profile "-o"  (.getAbsolutePath profile-plist))
      {:provisioning-profile-plist profile-plist})
    (catch Exception e
      {:err e :message "Failed to convert provisioning profile."})))

(g/defnk extract-entitlements-plist [^File provisioning-profile-plist]
  (try
    (let [profile-info (XMLPropertyListConfiguration.)
          entitlements-info (XMLPropertyListConfiguration.)
          entitlements-plist (fs/create-temp-file! "entitlement" ".xcent")]
      (with-open [r (io/reader provisioning-profile-plist)]
        (.load profile-info r))
      (.append entitlements-info (.configurationAt profile-info "Entitlements"))
      (.save entitlements-info entitlements-plist)
      {:entitlements-plist entitlements-plist})
    (catch Exception e
      {:err e :message "Failed to extract entitlements from provisioning profile."})))

(g/defnk make-info-plist [props]
  (try
    (let [plist-file (fs/create-temp-file! "Info.plist" "")
          plist (XMLPropertyListConfiguration.)]
      (with-open [r (io/reader (io/resource "bundle/ios/Info-dev-app.plist"))]
        (.load plist r))
      (doseq [[k v]  props]
        (.setProperty plist k v))
      (.save plist plist-file)
      {:info-plist plist-file})
    (catch Exception e
      {:err e :message "Failed to create Info.plist."})))

(g/defnk copy-launch-images [^File app-dir]
  (try
    (doseq [launch-image ["Default.png"
                          "Default@2x.png"
                          "Default-Portrait-812h@3x.png"
                          "Default-Portrait-736h@3x.png"
                          "Default-Portrait-667h@2x.png"
                          "Default-Portrait-1366h@2x.png"
                          "Default-Portrait-1024h@2x.png"
                          "Default-Portrait-1024h.png"
                          "Default-Landscape-812h@3x.png"
                          "Default-Landscape-736h@3x.png"
                          "Default-Landscape-667h@2x.png"
                          "Default-Landscape-1366h@2x.png"
                          "Default-Landscape-1024h@2x.png"
                          "Default-Landscape-1024h.png"
                          "Default-568h@2x.png"]]
      (with-open [launch-image-stream (io/input-stream (io/resource (str "bundle/ios/" launch-image)))]
        (io/copy launch-image-stream (io/file app-dir launch-image)))
      {:copied-launch-images true})
    (catch Exception e
      {:err e :message "Failed to copy launch images for bundling."})))

(g/defnk setup-fs-env []
  (try
    (let [package-dir (fs/create-temp-directory!)
          payload-dir (io/file package-dir "Payload")
          app-dir (io/file payload-dir "Defold.app")]
      (fs/create-directories! app-dir)
      {:package-dir package-dir
       :payload-dir payload-dir
       :app-dir app-dir})
    (catch Exception e
      {:err e :message "Failed to create directories for bundling."})))

(g/defnk upload-ipa [^File ipa cr-project-id prefs]
  (try
    (let [client (client/make-client prefs)]
      (when-let [user-id (client/user-id client)]
        (with-open [in (io/input-stream ipa)]
          (try
            (client/upload-engine client user-id cr-project-id "ios" in)
            {:uploaded true}
            (catch ExceptionInfo e
              (let [message (if (= 403 (:status (ex-data e)))
                              "You are not authorized to upload a signed ipa to the project dashboard."
                              "Failed to upload a signed ipa to the project dashboard.")]
                {:err e
                 :message message}))))))
    (catch Exception e
      {:err e :message "Failed to upload a signed ipa to the project dashboard."})))

(g/defnk noop [])

(handler/defhandler ::sign :dialog
  (enabled? [controls] (and (ui/selection (:identities controls))
                            (.exists (io/file (ui/text (:provisioning-profile controls))))
                            (.isDirectory (io/file (ui/text (:build-dir controls))))))
  (run [workspace prefs ^Stage stage root controls project result]
    (let [ipa-dir (ui/text (:build-dir controls))
          ipa (format "%s/%s.ipa" ipa-dir name)
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
          profile (ui/text (:provisioning-profile controls))
          cr-project-id (cr-project-id workspace)]
      (prefs/set-prefs prefs "last-identity" identity)
      (prefs/set-prefs prefs "last-provisioning-profile" profile)
      (prefs/set-prefs prefs "last-ios-build-dir" ipa-dir)
      ;; if project hosted by us, make sure we're logged in first
      (when (or (nil? cr-project-id)
                (login/login prefs))
        (ui/disable! root true)
        (let [initial-env {:ipa-file (io/file ipa)
                           :cr-project-id cr-project-id
                           :project project
                           :profile profile
                           :identity-id identity-id
                           :prefs prefs
                           :props props}
              success "Successfully uploaded a signed ipa to the project dashboard. Team members can download it to their device from the Settings page."
              sign-steps [setup-fs-env
                          make-provisioning-profile-plist
                          extract-entitlements-plist
                          make-info-plist
                          copy-launch-images
                          get-armv7-engine
                          get-arm64-engine
                          lipo-ios-engine
                          assemble-ios-app
                          sign-ios-app
                          package-ipa
                          ;; only upload if hosted by us
                          (if cr-project-id upload-ipa noop)
                          (if cr-project-id (g/fnk [] (dialogs/make-alert-dialog success)) noop)]]
          (loop [steps sign-steps
                 env initial-env]
            (when-let [step (first steps)]
              (assert (every? env (-> step meta :arguments)))
              (let [step-result (step env)]
                (if-let [error (:err step-result)]
                  (reset! result {:error error :message (:message step-result)})
                  (recur (next steps)
                         (merge env step-result))))))
          (ui/close! stage))))))

(defn make-sign-dialog [workspace prefs project]
  (let [root ^Parent (ui/load-fxml "sign-dialog.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["identities" "sign" "provisioning-profile" "provisioning-profile-button" "build-dir" "build-dir-button"])
        identities (find-identities)
        result (atom nil)]

    (ui/context! root :dialog {:root root :workspace workspace :prefs prefs :controls controls :stage stage :project project :result result} nil)
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
    (ui/show-and-wait! stage)
    @result))
