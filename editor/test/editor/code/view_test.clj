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

(ns editor.code.view-test
  (:require [clojure.test :refer [deftest is testing]]
            [editor.code.view :as view]
            [editor.ui :as ui])
  (:import [com.defold.control ZoomPane]
           [javafx.geometry Point2D]
           [javafx.scene Scene]
           [javafx.scene.canvas Canvas]
           [javafx.scene.layout Pane]
           [javafx.stage Screen Stage StageStyle]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Navigation choosers must follow the scaled caret and fit their enlarged
;; bounds on screen, including when unscaled popup dimensions would still fit.
(deftest navigation-popup-position-test
  (ui/run-now
    (let [screen-bounds (.getVisualBounds (Screen/getPrimary))
          window-width (* 0.5 (.getWidth screen-bounds))
          window-height (* 0.5 (.getHeight screen-bounds))
          popup-width (* 0.125 (.getWidth screen-bounds))
          popup-height (* 0.125 (.getHeight screen-bounds))]
      (doseq [scale [1.0 1.5 2.0]]
        (testing (str "UI scale " scale)
          (let [scale (double scale)
                canvas (Canvas. (/ window-width scale) (/ window-height scale))
                content (doto (Pane.) (-> .getChildren (.add canvas)))
                root (doto (ZoomPane. scale) (.setContent content))
                stage (doto (Stage. StageStyle/UNDECORATED)
                        (.setScene (Scene. root window-width window-height))
                        (.setX (+ (.getMinX screen-bounds) (* 0.5 window-width)))
                        (.setY (+ (.getMinY screen-bounds) (* 0.5 window-height))))]
            (try
              (.show stage)
              (testing "caret alignment away from screen edges"
                (let [cursor-bottom (Point2D. (* 0.25 (.getWidth canvas)) (* 0.25 (.getHeight canvas)))
                      expected (.localToScreen canvas cursor-bottom)
                      ^Point2D actual (#'view/pref-suggestions-popup-position canvas popup-width popup-height cursor-bottom)]
                  (is (< (.distance expected actual) 0.000001))))
              (testing "scaled popup fits near the bottom-right screen edge"
                (.setX stage (+ (.getMinX screen-bounds) window-width))
                (.setY stage (+ (.getMinY screen-bounds) window-height))
                (let [cursor-bottom (Point2D. (* 0.7 (.getWidth canvas)) (* 0.7 (.getHeight canvas)))
                      cursor-screen (.localToScreen canvas cursor-bottom)
                      ^Point2D actual (#'view/pref-suggestions-popup-position canvas popup-width popup-height cursor-bottom)
                      scaled-width (* scale popup-width)
                      scaled-height (* scale popup-height)]
                  (is (.contains screen-bounds (.getX cursor-screen) (.getY cursor-screen) popup-width popup-height))
                  (is (= (> scale 1.0)
                         (not (.contains screen-bounds (.getX cursor-screen) (.getY cursor-screen) scaled-width scaled-height))))
                  (is (.contains screen-bounds (.getX actual) (.getY actual) scaled-width scaled-height))))
              (finally
                (.hide stage)))))))))
