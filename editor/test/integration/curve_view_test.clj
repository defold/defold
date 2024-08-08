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

(ns integration.curve-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.camera :as camera]
            [editor.curve-view :as curve-view]
            [editor.handler :as handler]
            [editor.scene-selection :as selection]
            [editor.types :as types]
            [integration.test-util :as test-util])
  (:import [editor.curve_view SubSelectionProvider]
           [javax.vecmath Point3d]))

(defn- world->screen [view x y]
  (let [world-p (Point3d. x y 0.0)
        camera (g/node-value view :camera)
        viewport (g/node-value view :viewport)
        screen-p (camera/camera-project camera viewport world-p)]
    [(.x screen-p) (.y screen-p)]))

(defn- mouse-move! [view world-x world-y]
  (let [[x y] (world->screen view world-x world-y)]
    (test-util/mouse-move! view x y)))

(defn- mouse-press! [view world-x world-y]
  (let [[x y] (world->screen view world-x world-y)]
    (test-util/mouse-press! view x y)))

(defn- mouse-release! [view world-x world-y]
  (let [[x y] (world->screen view world-x world-y)]
    (test-util/mouse-release! view x y)))

(defn- mouse-click!
  ([view world-x world-y]
   (mouse-click! view world-x world-y []))
  ([view world-x world-y modifiers]
   (let [[x y] (world->screen view world-x world-y)]
     (test-util/mouse-click! view x y modifiers))))

(defn- mouse-dbl-click!
  ([view world-x world-y]
   (mouse-dbl-click! view world-x world-y []))
  ([view world-x world-y modifiers]
   (let [[x y] (world->screen view world-x world-y)]
     (test-util/mouse-dbl-click! view x y modifiers))))

(defn- mouse-drag! [view world-x0 world-y0 world-x1 world-y1]
  (let [[x0 y0] (world->screen view world-x0 world-y0)
        [x1 y1] (world->screen view world-x1 world-y1)]
    (test-util/mouse-drag! view x0 y0 x1 y1)))

(defn- sub-selection [app-view node-id property]
  (->> (g/node-value app-view :sub-selection)
    (filterv (fn [[nid prop sub-sel]] (and (= node-id nid) (= property prop) sub-sel)))
    (mapv last)))

(defn- make-curve-view! [app-view width height]
  (doto (curve-view/make-view! app-view (test-util/make-view-graph!) nil nil {} false)
    (g/set-property! :viewport (types/->Region 0 width 0 height))))

(deftest selection
  (test-util/with-loaded-project
    (let [curve-view (make-curve-view! app-view 400 400)
          node-id (test-util/open-tab! project app-view "/particlefx/fireworks_big.particlefx")
          emitter (:node-id (test-util/outline node-id [0]))
          modifier (first selection/toggle-modifiers)]
      (app-view/select! app-view [emitter])
      (are [x y mods selection] (do
                                  (mouse-click! curve-view x y mods)
                                  (= selection (sub-selection app-view emitter :particle-key-alpha)))
        0.0 0.0 [] [1]
        0.11 0.99 [] [2]
        0.0 0.0 [modifier] [2 1]
        0.11 0.99 [modifier] [1]))))

(defn- cp [nid property idx]
  (let [c (g/node-value nid property)]
    (some-> (g/node-value nid property)
      (types/geom-aabbs [idx])
      (get idx)
      first
      (subvec 0 2))))

(defn- cp? [exp act]
  (if act
    (let [delta (mapv - exp act)]
      (->> (mapv * delta delta)
        (reduce +)
        (Math/sqrt)
        (> 1.0E-2)))
    false))

(deftest move-control-point
  (test-util/with-loaded-project
    (let [curve-view (make-curve-view! app-view 800 400)
          node-id (test-util/open-tab! project app-view "/particlefx/fireworks_big.particlefx")
          emitter (:node-id (test-util/outline node-id [0]))
          original-curve (g/node-value emitter :particle-key-alpha)]
      (app-view/select! app-view [emitter])
      (mouse-move! curve-view -100.0 -100.0)
      (mouse-move! curve-view 0.0 0.0)
      (mouse-drag! curve-view 0.0 0.0 0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      ;; X limit for first control point
      (mouse-drag! curve-view 0.0 0.1 0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      (mouse-drag! curve-view 0.0 0.1 -0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      ;; Box selection
      (mouse-drag! curve-view -1.0 -1.0 0.2 1.0)
      ;; Multi-selection movement
      (mouse-drag! curve-view 0.0 0.1 0.0 0.2)
      (is (cp? [0.00 0.20] (cp emitter :particle-key-alpha 1)))
      (is (cp? [0.11 1.09] (cp emitter :particle-key-alpha 2)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      ;; X limit for second control point
      (mouse-drag! curve-view 0.11 1.09 -0.1 1.09)
      (is (< (first (cp emitter :particle-key-alpha 1))
             (first (cp emitter :particle-key-alpha 2))))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha)))))

(deftest add-delete-control-point
  (test-util/with-loaded-project
    (let [curve-view (make-curve-view! app-view 800 400)
          node-id (test-util/open-tab! project app-view "/particlefx/fireworks_big.particlefx")
          emitter (:node-id (test-util/outline node-id [0]))
          context (handler/->context :curve-view {} (SubSelectionProvider. app-view))
          original-curve (g/node-value emitter :particle-key-alpha)]
      (app-view/select! app-view [emitter])
      ; First control point can't be deleted
      (mouse-dbl-click! curve-view 0.0 0.0)
      (is (cp? [0.0 0.0] (cp emitter :particle-key-alpha 1)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      ; But second can
      (mouse-dbl-click! curve-view 0.05 0.5)
      (is (cp? [0.05 0.62] (cp emitter :particle-key-alpha 9)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      (mouse-dbl-click! curve-view 0.05 0.62)
      (is (nil? (cp emitter :particle-key-alpha 9)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha))
      ; Delete through handler
      (mouse-drag! curve-view 0.0 -2.0 1.0 2.0)
      (test-util/handler-run :delete [context] {})
      (is (every? (fn [i] (nil? (cp emitter :particle-key-alpha (+ i 2)))) (range 6)))
      (test-util/ensure-number-type-preserving! original-curve (g/node-value emitter :particle-key-alpha)))))
