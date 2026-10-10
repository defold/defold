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
