;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.ios-deploy
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [editor.fs :as fs]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process])
  (:import [clojure.lang IReduceInit]
           [java.io PushbackReader]))

(defn get-ios-deploy-path
  "Find ios-deploy path or throw if it's not found

  Performs file IO"
  [prefs]
  (if-let [prefs-path (not-empty (prefs/get prefs [:tools :ios-deploy-path]))]
    (or (fs/existing-path prefs-path)
        (throw (ex-info (format "ios-deploy path defined in preferences does not exist: '%s'" prefs-path)
                        {:path prefs-path})))
    (if (= :macos (os/os))
      (or
        (try (fs/existing-path (process/exec! "which" "ios-deploy")) (catch Exception _))
        (fs/existing-path "/opt/homebrew/bin/ios-deploy")
        (throw (ex-info "Could not find 'ios-deploy' on this system, please install it and try again.\nIf it's already installed, configure its path in the Preferences' Tools pane." {})))
      (throw (ex-info "ios-deploy is only available on macOS" {})))))

(defn- json-stream [input & {:as json-opts}]
  (reify IReduceInit
    (reduce [_ f init]
      (with-open [reader (PushbackReader. (io/reader input) 64)]
        (let [eof-value reader]
          (loop [acc init]
            (let [json (apply json/read reader (into [] cat (assoc json-opts
                                                              :eof-error? false
                                                              :eof-value eof-value)))]
              (if (= json eof-value)
                acc
                (let [acc (f acc json)]
                  (if (reduced? acc)
                    @acc
                    (recur acc)))))))))))

(defn list-devices!
  "Query and return a list of iOS devices connected to this system

  Will block for at least a second"
  [ios-deploy-path]
  (let [process (process/start! ios-deploy-path "--detect" "--json" "--timeout" "1")
        result (into []
                     (keep (fn [{:keys [Event Device]}]
                             (when (= Event "DeviceDetected")
                               {:id (:DeviceIdentifier Device)
                                :label (:DeviceName Device)})))
                     (json-stream (process/out process) :key-fn keyword))]
    (when-not (zero? (process/await-exit-code process))
      (throw (ex-info "Failed to list devices" {:ios-deploy-path ios-deploy-path})))
    result))

(defn install!
  "Install the build app on iOS device and block until the installation done

  Returns nil on success, throws on failure

  Args:
    ios-deploy-path    path to ios-deploy command, can be obtained using
                       [[get-ios-deploy-path]]
    device             iOS device to install the app on, can be obtained using
                       [[list-devices!]]
    app-path           path to a built .app bundle, a String or File
    out                an OutputStream instance used for piping the installation
                       output, will not be closed after use"
  [ios-deploy-path device app-path out]
  {:pre [(string? ios-deploy-path) (string? (:id device)) (fs/existing-path app-path)]}
  (let [process (process/start! {:err :stdout} ios-deploy-path
                                "--id" (:id device)
                                "--bundle" (str app-path))]
    (process/pipe! (process/out process) out)
    (when-not (zero? (process/await-exit-code process))
      (throw (ex-info "Failed to install the app"
                      {:ios-deploy-path ios-deploy-path
                       :device device
                       :app-path app-path})))))

(defn launch!
  "Launch installed app on iOS device and block until the launch is done

  Returns nil on success, throws on failure

  Args:
    ios-deploy-path    path to ios-deploy command, can be obtained using
                       [[get-ios-deploy-path]]
    device             iOS device to install the app on, can be obtained using
                       [[list-devices!]]
    app-path           path to a built .app bundle, a String or File
    out                an OutputStream instance used for piping the installation
                       output, will not be closed after use"
  [ios-deploy-path device app-path out]
  {:pre [(string? ios-deploy-path) (string? (:id device)) (fs/existing-path app-path)]}
  (let [process (process/start! {:err :stdout} ios-deploy-path
                                "--id" (:id device)
                                "--bundle" (str app-path)
                                "--justlaunch"
                                "--noinstall")]
    (process/pipe! (process/out process) out)
    (when-not (zero? (process/await-exit-code process))
      (throw (ex-info "Failed to launch the app"
                      {:ios-deploy-path ios-deploy-path
                       :device device
                       :app-path app-path})))))