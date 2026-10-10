;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.os
  (:require [clojure.string :as str])
  (:import [com.dynamo.bob Platform]
           [java.lang ProcessBuilder]
           [java.io BufferedReader InputStreamReader]))

(defn- os-raw []
  (keyword (.. Platform getHostPlatform getOs)))

(def ^{:arglists '([])} os
  "Returns either :win32, :macos or :linux"
  (memoize os-raw))

(defn is-mac-os? []
  (= (os) :macos))

(defn is-linux? []
  (= (os) :linux))

(defn is-win32? []
  (= (os) :win32))

(defn is-wayland? []
  (and (is-linux?)
       (or (some? (System/getenv "WAYLAND_DISPLAY"))
           (= "wayland" (System/getenv "XDG_SESSION_TYPE")))))

;; System theme detection via platform-specific commands
(defn- run-command [cmd]
  (try
    (let [proc (-> (ProcessBuilder. cmd) (.redirectErrorStream true) .start)
          reader (BufferedReader. (InputStreamReader. (.getInputStream proc)))
          output (slurp reader)]
      (.waitFor proc)
      (str/trim output))
    (catch Exception _
      nil)))

(defn- macos-dark-mode? []
  (when (is-mac-os?)
    (let [output (run-command ["defaults" "read" "-g" "AppleInterfaceStyle"])]
      (= "Dark" output))))

(defn- windows-dark-mode? []
  (when (is-win32?)
    (let [output (run-command ["reg" "query" "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize" "/v" "AppsUseLightTheme"])]
      (when output
        (let [value (last (str/split output #"\s+"))]
          (= "0x0" value))))))

(defn- linux-dark-mode? []
  (when (is-linux?)
    (or
      ;; GNOME
      (let [output (run-command ["gsettings" "get" "org.gnome.desktop.interface" "color-scheme"])]
        (when output
          (some-> (re-find #"dark" output) boolean)))
      ;; KDE
      (let [output (run-command ["kreadconfig5" "--group" "Colors" "--key" "ColorScheme" 2>/dev/null])]
        (when output
          (some-> (re-find #"(?i)dark" output) boolean)))
      ;; Fallback: GTK_THEME env var
      (let [gtk-theme (System/getenv "GTK_THEME")]
        (when gtk-theme
          (some-> (re-find #"(?i)dark" gtk-theme) boolean))))))

(defn system-dark-mode? []
  "Returns true if the system is in dark mode, false otherwise"
  (cond
    (is-mac-os?) (macos-dark-mode?)
    (is-win32?) (windows-dark-mode?)
    (is-linux?) (linux-dark-mode?)
    :else false))

(defn system-theme []
  "Returns :dark or :light based on system preference"
  (if (system-dark-mode?) :dark :light))