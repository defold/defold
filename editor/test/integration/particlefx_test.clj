(ns integration.particlefx-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.particlefx :as particlefx]
            [editor.particle-lib :as plib]
            [integration.test-util :as test-util])
  (:import [editor.types Region]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]))

(defn- dump-outline [outline]
  {:_self (type (:_self outline)) :children (map dump-outline (:children outline))})

(deftest basic
  (testing "Basic scene"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node-id   (test-util/resource-node project "/particlefx/default.particlefx")
                   scene (g/node-value node-id :scene)]
               (is (= 1 (count (:children scene))))))))

(deftest modifiers
  (testing "Basic scene"
           (with-clean-system
             (let [workspace (test-util/setup-workspace! world)
                   project   (test-util/setup-project! workspace)
                   node-id   (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
                   outline (g/node-value node-id :node-outline)]
               (is (= 4 (count (:children outline))))
               (let [mod-drag (get-in outline [:children 3 :node-id])
                     props (:properties (g/node-value mod-drag :_properties))]
                 (doseq [key [:magnitude :max-distance]]
                   (is (not= nil (get-in props [key :value])))))))))

(deftest simulation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/particlefx/default.particlefx")
          prototype-msg (g/node-value node-id :rt-pb-data)
          emitter-sim-data (g/node-value node-id :emitter-sim-data)
          fetch-anim-fn (fn [index] (get emitter-sim-data index))
          transforms [(doto (Matrix4d.) (.setIdentity))]
          sim (plib/make-sim 16 256 prototype-msg transforms)]
      (testing "Sim sleeping"
               (is (plib/sleeping? sim))
               (plib/simulate sim 1/60 fetch-anim-fn transforms)
               (is (not (plib/sleeping? sim))))
      (testing "Stats"
               (let [stats (-> sim
                             (plib/simulate 1/60 fetch-anim-fn transforms)
                             (plib/simulate 1/60 fetch-anim-fn transforms)
                             (plib/stats))]
                 (is (< 0 (:particles (plib/stats sim))))))
      (testing "Rendering"
               (plib/render sim (fn [texture-index blend-mode v-index v-count]
                                  (is (= 12 v-count)))))
      (testing "Dispose"
               (plib/destroy-sim sim)))))

(deftest particlefx-validation
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/particlefx/default.particlefx")
          emitter (:node-id (test-util/outline node-id [0]))]
      (is (nil? (test-util/prop-error emitter :tile-source)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.atlas")]]
        (test-util/with-prop [emitter :tile-source v]
          (is (g/error? (test-util/prop-error emitter :tile-source)))))
      (is (nil? (test-util/prop-error emitter :material)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
        (test-util/with-prop [emitter :material v]
          (is (g/error? (test-util/prop-error emitter :material)))))
      (is (nil? (test-util/prop-error emitter :animation)))
      (doseq [v ["" "not_found"]]
        (test-util/with-prop [emitter :animation v]
          (is (g/error? (test-util/prop-error emitter :animation))))))))

(deftest scene-hierarchy-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          particle-fx (test-util/resource-node project "/scene_hierarchy/particle_fx.particlefx")
          particle-fx-scene (g/node-value particle-fx :scene)]

      (testing "Particle fx scene"
        (is (= particle-fx (:node-id particle-fx-scene)))
        (is (= (:node-id particle-fx-scene) (:pick-id particle-fx-scene)))
        (is (true? (contains? particle-fx-scene :aabb)))
        (is (true? (contains? particle-fx-scene :renderable)))
        (is (false? (contains? particle-fx-scene :node-path)))
        (let [particle-fx-children (:children particle-fx-scene)]
          (is (= 2 (count particle-fx-children)))
          (testing "Emitter scene"
            (let [emitter-scene (test-util/find-child-scene particlefx/EmitterNode particle-fx-children)]
              (is (some? emitter-scene))
              (is (true? (contains? emitter-scene :aabb)))
              (is (true? (contains? emitter-scene :renderable)))
              (is (= (:node-id emitter-scene) (:pick-id emitter-scene)))
              (is (= [(:node-id emitter-scene)] (:node-path emitter-scene)))
              (let [emitter-children (:children emitter-scene)]
                (is (= 1 (count emitter-children)))
                (testing "Local modifier scene"
                  (let [local-modifier-scene (test-util/find-child-scene particlefx/ModifierNode emitter-children)]
                    (is (some? local-modifier-scene))
                    (is (true? (contains? local-modifier-scene :aabb)))
                    (is (true? (contains? local-modifier-scene :renderable)))
                    (is (= (:node-id emitter-scene) (:pick-id local-modifier-scene)))
                    (is (= [(:node-id emitter-scene) (:node-id local-modifier-scene)] (:node-path local-modifier-scene))))))))
          (testing "Global modifier scene"
            (let [global-modifier-scene (test-util/find-child-scene particlefx/ModifierNode particle-fx-children)]
              (is (some? global-modifier-scene))
              (is (true? (contains? global-modifier-scene :aabb)))
              (is (true? (contains? global-modifier-scene :renderable)))
              (is (= (:node-id global-modifier-scene) (:pick-id global-modifier-scene)))
              (is (= [(:node-id global-modifier-scene)] (:node-path global-modifier-scene))))))))))
