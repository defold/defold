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

(ns editor.colors-test
  (:require [clojure.test :refer [deftest is use-fixtures]]
            [editor.colors :as colors])
  )

(def ^:private dark-scene-background (colors/hex-color->color "#292A2F"))
(def ^:private light-scene-background (colors/hex-color->color "#FFFFFF"))
(def ^:private ruler-label (colors/hex-color->color "#4A4A4A"))
(def ^:private outline (colors/hex-color->color "#666666"))

(defn- restore-dark-scene-palette-fixture
  "Restores the default dark palette after each test so the theme a test
  applies cannot leak into unrelated test namespaces."
  [f]
  (colors/apply-scene-theme! :dark)
  (f)
  (colors/apply-scene-theme! :dark))

(use-fixtures :each restore-dark-scene-palette-fixture)

;; Verifies apply-scene-theme! swaps the GL scene palette Vars to the light
;; values. Render functions read these Vars every frame, so a wrong value
;; would paint scene, image and curve views with the wrong background.
(deftest apply-scene-theme-light-swaps-palette
  (colors/apply-scene-theme! :light)
  (is (= light-scene-background colors/scene-background))
  (is (= ruler-label colors/scene-ruler-label))
  (is (= outline colors/outline-color)))

;; Verifies apply-scene-theme! restores the dark palette values, which are
;; the shipped defaults. Guards against a light palette value leaking into a
;; dark-theme session after cycling themes.
(deftest apply-scene-theme-dark-restores-palette
  (colors/apply-scene-theme! :light)
  (colors/apply-scene-theme! :dark)
  (is (= dark-scene-background colors/scene-background))
  (is (= colors/bright-grey colors/scene-ruler-label))
  (is (= colors/bright-grey colors/outline-color)))

;; Verifies both palettes agree on the grid axis colors: the axes are
;; intentionally theme-independent (colored, alpha-blended), so a theme
;; change must not blank them out.
(deftest grid-axis-colors-are-theme-independent
  (colors/apply-scene-theme! :light)
  (let [light-x colors/scene-grid-x-axis]
    (colors/apply-scene-theme! :dark)
    (is (= light-x colors/scene-grid-x-axis))))
