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
  (:import [com.dynamo.bob Platform]))

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
(defn- run-command
  "Runs a command and returns its trimmed stdout, or nil if the command
  could not be executed or exited with a non-zero code."
  [cmd]
  (try
    (let [proc (-> (ProcessBuilder. cmd) (.redirectErrorStream true) .start)
          output (slurp (.getInputStream proc))
          exit-code (.waitFor proc)]
      (when (zero? exit-code)
        (str/trim output)))
    (catch Exception _
      nil)))

(defn- macos-dark-mode? []
  ;; Prints "Dark" in dark mode; errors (exit 1) in light mode.
  (= "Dark" (run-command ["defaults" "read" "-g" "AppleInterfaceStyle"])))

(defn- windows-dark-mode? []
  ;; AppsUseLightTheme is REG_DWORD 0x0 in dark mode, 0x1 in light mode.
  (when-some [output (run-command ["reg" "query"
                                   "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"
                                   "/v" "AppsUseLightTheme"])]
    (= "0x0" (last (str/split output #"\s+")))))

(defn- linux-dark-mode? []
  (boolean
    (or
      ;; GNOME (outputs 'prefer-dark' or 'default')
      (when-some [output (run-command ["gsettings" "get" "org.gnome.desktop.interface" "color-scheme"])]
        (re-find #"dark" output))
      ;; KDE Plasma
      (when-some [output (run-command ["kreadconfig5" "--group" "General" "--key" "ColorScheme"])]
        (re-find #"(?i)dark" output))
      ;; Fallback: GTK_THEME environment variable (e.g. "Adwaita:dark")
      (when-some [gtk-theme (System/getenv "GTK_THEME")]
        (re-find #"(?i)dark" gtk-theme)))))

(defn system-dark-mode?
  "Returns true if the operating system is currently in dark mode."
  []
  (cond
    (is-mac-os?) (macos-dark-mode?)
    (is-win32?) (windows-dark-mode?)
    (is-linux?) (linux-dark-mode?)
    :else false))

(defn system-theme
  "Returns :dark or :light based on the current operating system theme."
  []
  (if (system-dark-mode?) :dark :light))