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
  (:require [clojure.test :refer :all]
            [editor.code.view :as view])
  (:import [javafx.geometry Rectangle2D]))

(defn- screen [min-x width]
  (Rectangle2D. min-x 0.0 width 1080.0))

;; The doc popup goes right of the completion list when it fits there, and
;; otherwise to whichever side has more space, shrinking to fit.
(deftest completion-doc-placement-side-test
  (is (= {:x 406.0 :width 450.0 :side :right}
         (view/completion-doc-placement (screen 0.0 1920.0) 100.0 300.0 450.0)))
  (is (= {:x 944.0 :width 450.0 :side :left}
         (view/completion-doc-placement (screen 0.0 1920.0) 1400.0 300.0 450.0)))
  (is (= {:x 1106.0 :width 414.0 :side :right}
         (view/completion-doc-placement (screen 0.0 1520.0) 100.0 1000.0 450.0)))
  (is (= {:x 0.0 :width 204.0 :side :left}
         (view/completion-doc-placement (screen 0.0 700.0) 210.0 300.0 450.0))))

;; Placement uses the bounds of the monitor the list is on, guarding against
;; measuring the left space from x = 0 on monitors left or right of the
;; primary one.
(deftest completion-doc-placement-secondary-monitor-test
  (is (= {:x -956.0 :width 450.0 :side :left}
         (view/completion-doc-placement (screen -1920.0 1920.0) -500.0 300.0 450.0)))
  (is (= {:x 3506.0 :width 334.0 :side :right}
         (view/completion-doc-placement (screen 1920.0 1920.0) 2000.0 1500.0 450.0))))

;; A popup that exactly fits against the screen edge keeps its preferred width.
(deftest completion-doc-placement-exact-fit-test
  (is (= {:x 1470.0 :width 450.0 :side :right}
         (view/completion-doc-placement (screen 0.0 1920.0) 1164.0 300.0 450.0)))
  (is (= {:x 0.0 :width 450.0 :side :left}
         (view/completion-doc-placement (screen 0.0 1920.0) 456.0 1400.0 450.0))))
