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

(ns editor.colors)

(set! *warn-on-reflection* true)

(defn- hex-color->color [str]
  (let [conv (fn [s] (/ (Integer/parseInt s 16) 255.0))]
    [(conv (subs str 1 3))
     (conv (subs str 3 5))
     (conv (subs str 5 7))
     1.0]))

(def defold-light-blue (hex-color->color "#60a8ff"))
(def defold-blue (hex-color->color "#0084ff"))
(def defold-white (hex-color->color "#f0f2f6"))
(def defold-white-light (hex-color->color "#f8f8fb"))
(def defold-red (hex-color->color "#ff0000"))
(def defold-yellow (hex-color->color "#fbce2f"))
(def defold-turquoise (hex-color->color "#00e6e1"))
(def defold-pink (hex-color->color "#E200E6"))
(def defold-green (hex-color->color "#3cff00"))
(def defold-orange (hex-color->color "#fd6623"))
(def selected-blue (hex-color->color "#264a8b"))
(def bright-black (hex-color->color "#292c30"))
(def bright-black-light (hex-color->color "#2e3236"))
(def mid-black (hex-color->color "#25282b"))
(def mid-black-light (hex-color->color "#292d31"))
(def dark-black (hex-color->color "#222426"))
(def dark-black-light (hex-color->color "#26282a"))
(def dark-grey (hex-color->color "#34373b"))
(def dark-grey-light (hex-color->color "#3b3e43"))
(def mid-grey (hex-color->color "#494b4e"))
(def mid-grey-light (hex-color->color "#54565a"))
(def bright-grey (hex-color->color "#8f9296"))
(def bright-grey-light (hex-color->color "#a8abaf"))

(defn alpha [c a]
  (assoc c 3 a))

(def input-background (hex-color->color "#292a2f"))
(def scene-background input-background)
(def scene-grid dark-grey)
(def scene-grid-x-axis (alpha defold-red 0.4))
(def scene-grid-y-axis (alpha defold-green 0.4))
(def scene-grid-z-axis (alpha defold-blue 0.4))
(def scene-ruler-label bright-grey)
(def scene-ruler-marker bright-grey)

(def outline-color bright-grey)
(def selected-outline-color defold-turquoise)
(def parent-selected-outline-color defold-light-blue)

(defn selection-color [selection-state]
  (case selection-state
    :self-selected selected-outline-color
    :parent-selected parent-selected-outline-color
    nil))

(defn renderable-outline-color [renderable]
  (or (selection-color (:selected renderable))
      outline-color))

; https://en.wikipedia.org/wiki/HSL_and_HSV

(defn- hsc->rgb1 [h s c]
  (let [h' (/ h 60.0)
        x (* c (- 1.0 (Math/abs (- (mod h' 2.0) 1.0))))]
    (cond
      (< h' 1) [c x 0.0]
      (< h' 2) [x c 0.0]
      (< h' 3) [0.0 c x]
      (< h' 4) [0.0 x c]
      (< h' 5) [x 0.0 c]
      (< h' 6) [c 0.0 x])))

(defn hsv->rgb [h s v]
  (let [c (* s v)
        [r1 g1 b1] (hsc->rgb1 h s c)
        m (- v c)]
    [(+ r1 m) (+ g1 m) (+ b1 m)]))

(defn hsv->rgba [h s v]
  (alpha (hsv->rgb h s v) 1.0))

(defn hsl->rgb [h s l]
  (let [c (* s (- 1.0 (Math/abs (- (* 2.0 l) 1.0))))
        [r1 g1 b1] (hsc->rgb1 h s c)
        m (- l (* 0.5 c))]
    [(+ r1 m) (+ g1 m) (+ b1 m)]))

(defn hsl->rgba [h s l]
  (alpha (hsl->rgb h s l) 1.0))
