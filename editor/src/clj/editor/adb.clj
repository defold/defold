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

(ns editor.adb
  (:require [clojure.string :as string]
            [editor.fs :as fs]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]))

(defmacro attempt [expr]
  `(try ~expr (catch Exception _#)))

(defn- infer-adb-location! []
  (case (os/os)
    :macos (or
             (some-> (fs/evaluate-path "$ANDROID_HOME/platform-tools/adb") fs/existing-path)
             (some-> (attempt (process/exec! "which" "adb")) fs/existing-path)
             (some-> (fs/evaluate-path "~/Library/Android/sdk/platform-tools/adb") fs/existing-path)
             (fs/existing-path "/opt/homebrew/bin/adb"))
    :linux (or
             (some-> (fs/evaluate-path "$ANDROID_HOME/platform-tools/adb") fs/existing-path)
             (some-> (attempt (process/exec! "which" "adb")) fs/existing-path)
             (some-> (fs/evaluate-path "~/Android/Sdk/platform-tools/adb") fs/existing-path)
             (fs/existing-path "/usr/bin/adb")
             (fs/existing-path "/usr/lib/android-sdk/platform-tools/adb"))
    :win32 (or
             (some-> (fs/evaluate-path "$ANDROID_HOME\\platform-tools\\adb.exe") fs/existing-path)
             (some-> (attempt (process/exec! "where" "adb.exe")) fs/existing-path)
             (some-> (fs/evaluate-path "~\\AppData\\Local\\Android\\sdk\\platform-tools\\adb.exe") fs/existing-path)
             (fs/existing-path "C:\\Program Files (x86)\\Android\\android-sdk\\platform-tools\\adb.exe"))))

(defn get-adb-path
  "Find ADB path (a string) or throw if it wasn't found

  Performs file IO"
  [prefs]
  (if-let [prefs-path (not-empty (prefs/get prefs [:tools :adb-path]))]
    (or (fs/existing-path prefs-path)
        (throw (ex-info (str "ADB path defined in preferences does not exist: '" prefs-path "'")
                        {:path prefs-path})))
    (or (infer-adb-location!)
        (throw (ex-info "Could not find ADB on this system, please install it and try again.
If it's already installed, configure its path in the Preferences' Tools pane." {})))))

(defn list-devices!
  "Query and return a list of devices connected to this system

  Might block for seconds while ADB is starting up its daemon"
  [adb-path]
  (let [output (process/exec! adb-path "devices" "-l")]
    (into []
          (keep (fn [line]
                  (when-let [[_ id kvs] (re-matches #"^([^\s]+)\s+device\s(.+)" line)]
                    (let [{:strs [device model]} (into {}
                                                       (keep #(let [kv (string/split % #":" 2)]
                                                                (when (= 2 (count kv))
                                                                  kv)))
                                                       (string/split kvs #" "))]
                      {:id id
                       :label (cond
                                (and device model) (str device " " model)
                                device device
                                model model
                                :else id)}))))
          (string/split-lines output))))

(defn install!
  "Perform an APK install on a device and block until the installation is done

  Returns nil on success, throws exception on failure.

  Args:
    adb-path    path to adb command, can be obtained with [[get-adb-path]]
    device      device to install on, can be obtained with [[list-devices!]]
    apk-path    path to APK file to install, either String or File
    out         an OutputStream used for piping the installation output, will
                not be closed after use"
  [adb-path device apk-path out]
  {:pre [(string? (:id device))
         (fs/existing-path apk-path)]}
  (let [process (process/start! {:err :stdout} adb-path "-s" (:id device) "install" "-r" (str apk-path))]
    (process/pipe! (process/out process) out)
    (when-not (zero? (process/await-exit-code process))
      (throw (ex-info "Failed to install APK" {:adb adb-path :device device :apk apk-path})))))

(defn launch!
  "Perform an application launch and block until the launch is done
  
  Returns nil on success, throws exception on failure
  
  Args:
    adb-path    path to adb command, can be obtained with [[get-adb-path]]
    device      device to install on, can be obtained with [[list-devices!]]
    package     application package name to launch
    out         an OutputStream used for piping the launch output, will not be
                closed after use"
  [adb-path device package out]
  {:pre [(string? (:id device))]}
  (let [process (process/start! {:err :stdout}
                                adb-path
                                "-s" (:id device)
                                "shell" "am" "start"
                                "-n" (str package "/com.dynamo.android.DefoldActivity"))]
    (process/pipe! (process/out process) out)
    (when-not (zero? (process/await-exit-code process))
      (throw (ex-info "Failed to launch the app" {:adb adb-path :device device :app package})))))
