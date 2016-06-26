(ns integration.curve-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.camera :as camera]
            [editor.types :as types]
            [integration.test-util :as test-util])
  (:import [javax.vecmath Point3d]))

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

(defn- mouse-drag! [view world-x0 world-y0 world-x1 world-y1]
  (let [[x0 y0] (world->screen view world-x0 world-y0)
        [x1 y1] (world->screen view world-x1 world-y1)]
    (test-util/mouse-drag! view x0 y0 x1 y1)))

(defn- sub-selection [project node-id property]
  (->> (g/node-value project :sub-selection)
    (filterv (fn [[nid prop sub-sel]] (and (= node-id nid) (= property prop) sub-sel)))
    (mapv last)))

(deftest selection
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          curve-view (doto (curve-view/make-view! project (test-util/make-view-graph!) nil {} false)
                       (g/set-property! :viewport (types/->Region 0 400 0 400)))
          node-id (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
          emitter (:node-id (test-util/outline node-id [2]))]
      (project/select! project [emitter])
      (mouse-click! curve-view 0.0 0.0)
      (is (= [1] (sub-selection project emitter :particle-key-alpha)))
      (mouse-click! curve-view 0.11 0.99)
      (is (= [2] (sub-selection project emitter :particle-key-alpha)))
      (mouse-click! curve-view 0.0 0.0 [:shift])
      (is (= [2 1] (sub-selection project emitter :particle-key-alpha)))
      (mouse-click! curve-view 0.11 0.99 [:shift])
      (is (= [1] (sub-selection project emitter :particle-key-alpha))))))

(defn- cp [nid property idx]
  (let [c (g/node-value nid property)]
    (-> (g/node-value nid property)
      (types/geom-aabbs [idx])
      (get idx)
      first
      (subvec 0 2))))

(defn- cp? [exp act]
  (let [delta (mapv - exp act)]
    (->> (mapv * delta)
      (reduce +)
      (> 1.0E-10))))

(deftest move-control-point
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          curve-view (doto (curve-view/make-view! project (test-util/make-view-graph!) nil {} false)
                       (g/set-property! :viewport (types/->Region 0 800 0 400)))
          node-id (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
          emitter (:node-id (test-util/outline node-id [2]))]
      (project/select! project [emitter])
      (mouse-move! curve-view -100.0 -100.0)
      (mouse-move! curve-view 0.0 0.0)
      (mouse-drag! curve-view 0.0 0.0 0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (mouse-drag! curve-view 0.0 0.1 0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (mouse-drag! curve-view 0.0 0.1 -0.1 0.1)
      (is (cp? [0.0 0.1] (cp emitter :particle-key-alpha 1)))
      (mouse-drag! curve-view 0.11 0.99 -0.1 0.99)
      (is (< (first (cp emitter :particle-key-alpha 1))
             (first (cp emitter :particle-key-alpha 2)))))))
