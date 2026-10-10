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

(ns editor.os-test
  (:require [clojure.test :refer [deftest is]]
            [editor.os :as os]))

;; Verifies system-theme always resolves to one of the two theme keywords on
;; the running platform. Guards against a detection path returning nil or an
;; unexpected value, which would silently break theme selection.
(deftest system-theme-returns-valid-theme
  (is (contains? #{:dark :light} (os/system-theme))))

;; Verifies system-dark-mode? returns a strict boolean so callers can rely on
;; truthiness. Guards against a platform branch leaking a nil (e.g. from a
;; failed external command) or a regex match result out of the helpers.
(deftest system-dark-mode-returns-boolean
  (is (boolean? (os/system-dark-mode?))))

;; Verifies the supported-platform branches are exhaustive: os returns one of
;; :win32, :macos or :linux, and system-theme maps it onto a theme keyword.
;; Guards against adding a platform keyword without extending theme detection.
(deftest system-theme-covers-host-platform
  (is (contains? #{:win32 :macos :linux} (os/os)))
  (is (contains? #{:dark :light} (os/system-theme))))

;; Verifies the Windows registry parser detects dark mode from real `reg
;; query` output, including CRLF line endings and REG_DWORD formatting.
;; Guards against locale/format changes breaking dark-mode detection.
(deftest windows-light-theme-parses-reg-query-output
  (let [dark "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\r\n    AppsUseLightTheme    REG_DWORD    0x0\r\n\r\n"
        light "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\r\n    AppsUseLightTheme    REG_DWORD    0x1\r\n\r\n"]
    (is (true? (os/windows-light-theme? light)))
    (is (false? (os/windows-light-theme? dark)))
    (is (false? (os/windows-light-theme? "")))
    (is (false? (os/windows-light-theme? nil)))))

;; Verifies the GNOME color-scheme parser accepts the values gsettings emits
;; ('prefer-dark', 'default', 'prefer-light'). Guards against treating the
;; light schemes as dark.
(deftest gnome-color-scheme-parses-gsettings-output
  (is (true? (os/gnome-dark-color-scheme? "'prefer-dark'")))
  (is (false? (os/gnome-dark-color-scheme? "'default'")))
  (is (false? (os/gnome-dark-color-scheme? "'prefer-light'")))
  (is (false? (os/gnome-dark-color-scheme? "")))
  (is (false? (os/gnome-dark-color-scheme? nil))))

;; Verifies the KDE parser detects the dark suffix in ColorScheme names such
;; as BreezeDark and org.kde.breezedark.theme, case-insensitively. Guards
;; against KDE theme names without the exact "Dark" casing.
(deftest kde-color-scheme-parses-kreadconfig-output
  (is (true? (os/kde-dark-color-scheme? "BreezeDark")))
  (is (true? (os/kde-dark-color-scheme? "org.kde.breezedark.theme")))
  (is (false? (os/kde-dark-color-scheme? "BreezeLight")))
  (is (false? (os/kde-dark-color-scheme? "Breeze")))
  (is (false? (os/kde-dark-color-scheme? nil))))

;; Verifies the GTK_THEME fallback detects the :dark suffix used by GTK
;; theme names. Guards against the env-var fallback misdetecting light
;; themes.
(deftest gtk-theme-parses-dark-suffix
  (is (true? (os/gtk-theme-dark? "Adwaita:dark")))
  (is (true? (os/gtk-theme-dark? "Yaru-dark")))
  (is (false? (os/gtk-theme-dark? "Adwaita")))
  (is (false? (os/gtk-theme-dark? nil))))
