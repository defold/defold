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

(ns editor.ui-theme-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer [deftest is]]
            [editor.prefs :as prefs]
            [editor.ui :as ui]))

;; Verifies the theme stylesheet mapping covers every generated theme
;; stylesheet and only rewrites them when the light theme is selected.
;; Guards against the swap silently missing a stylesheet pair, which used to
;; leave the dark stylesheet active in light mode.
(deftest themed-stylesheet-urls-rewrites-theme-owned-urls
  (let [editor-dark (str (io/resource "editor.css"))
        editor-light (str (io/resource "editor-light.css"))
        dialogs-dark (str (io/resource "dialogs.css"))
        dialogs-light (str (io/resource "dialogs-light.css"))
        splash-dark (str (io/resource "splash.css"))
        splash-light (str (io/resource "splash-light.css"))
        user-css "file:///Users/dev/.defold/editor.css"]
    (is (= [editor-light dialogs-light splash-light user-css]
           (vec (ui/themed-stylesheet-urls
                  [editor-dark dialogs-dark splash-dark user-css] :light))))
    (is (= [editor-dark dialogs-dark splash-dark user-css]
           (vec (ui/themed-stylesheet-urls
                  [editor-dark dialogs-dark splash-dark user-css] :dark))))))

;; Verifies the rewrite also works from light back to dark, and that switching
;; dark -> light -> dark restores the original stylesheets. Guards against the
;; UI staying light after the user selects Dark (or the OS flips to dark) once
;; the light stylesheets have been applied.
(deftest themed-stylesheet-urls-rewrites-light-urls-back-to-dark
  (let [dark [(str (io/resource "editor.css"))
              (str (io/resource "dialogs.css"))
              "file:///Users/dev/.defold/editor.css"]
        light (vec (ui/themed-stylesheet-urls dark :light))]
    (is (not= dark light))
    (is (= dark (vec (ui/themed-stylesheet-urls light :dark))))))

;; Verifies every theme-owned stylesheet resolves to a real resource in both
;; themes. Guards against a missing generated light stylesheet turning
;; into an empty URL that the rewrite would silently pass around.
(deftest theme-stylesheets-exist
  (doseq [resource ["editor.css" "editor-light.css" "dialogs.css" "dialogs-light.css" "splash.css" "splash-light.css"]]
    (is (some? (io/resource resource)) resource)))

;; Verifies URLs that do not belong to the theme system pass through the
;; rewrite untouched in both themes. Guards against user or dialog
;; stylesheets being swapped or dropped on theme changes.
(deftest themed-stylesheet-urls-preserves-foreign-urls
  (let [foreign ["file:///somewhere/dialogs.css.bak" "https://example.test/theme.css"]]
    (is (= foreign (vec (ui/themed-stylesheet-urls foreign :light))))
    (is (= foreign (vec (ui/themed-stylesheet-urls foreign :dark))))))

;; Verifies the stylesheet resource names for both resolved themes: the
;; project window loads editor-light.css in light mode and editor.css in
;; dark mode. Guards against a typo leaving a theme without any stylesheet.
(deftest theme-css-resource-maps-resolved-themes
  (is (= "editor-light.css" (ui/theme-css-resource :light)))
  (is (= "editor.css" (ui/theme-css-resource :dark))))

;; Verifies the :system theme can be resolved from a thread that is not the
;; JavaFX application thread, and then resolves to the dark theme. Guards
;; against asking JavaFX for the color scheme off its thread, which throws:
;; the theme watcher does so from the preferences watch, where the exception
;; used to abort the write of the preferences.
(deftest system-theme-resolves-off-the-fx-thread
  (let [prefs-path (.toPath (io/file (System/getProperty "java.io.tmpdir")
                                     (str "ui-theme-test-" (random-uuid) ".editor_settings")))
        prefs (prefs/global prefs-path)]
    (try
      (prefs/set! prefs [:window :theme] :system)
      (ui/set-theme-prefs! prefs)
      (is (= :dark @(future (ui/current-resolved-theme))))
      (finally
        (ui/set-theme-prefs! nil)))))
