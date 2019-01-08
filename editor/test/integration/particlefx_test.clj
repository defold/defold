(ns integration.particlefx-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.material :as material]
            [editor.workspace :as workspace]
            [editor.types :as types]
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
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/particlefx/default.particlefx")
                   scene (g/node-value node-id :scene)]
               (is (= 1 (count (:children scene))))))))

(deftest modifiers
  (testing "Basic scene"
           (test-util/with-loaded-project
             (let [node-id   (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
                   outline (g/node-value node-id :node-outline)]
               (is (= 4 (count (:children outline))))
               (let [mod-drag (get-in outline [:children 3 :node-id])
                     props (:properties (g/node-value mod-drag :_properties))]
                 (doseq [key [:magnitude :max-distance]]
                   (is (not= nil (get-in props [key :value])))))))))

(deftest simulation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/particlefx/default.particlefx")
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
               (let [sim (-> sim
                             (plib/simulate 1/60 fetch-anim-fn transforms)
                             (plib/simulate 1/60 fetch-anim-fn transforms))
                     stats (do (plib/gen-emitter-vertex-data sim 0 [1.0 1.0 1.0 1.0])
                               (plib/stats sim))]
                 (is (< 0 (:particles (plib/stats sim))))))
      (testing "Rendering"
        (is (= 12 (:v-count (plib/render-emitter sim 0)))))
      (testing "Dispose"
               (plib/destroy-sim sim)))))

(deftest particlefx-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/particlefx/default.particlefx")
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

(deftest particle-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/particlefx/default.particlefx")
          material-node (project/get-resource-node project "/materials/test_samplers.material")          
          [emitter-a emitter-b] (g/node-value node-id :nodes)]
      (testing "uses shader and texture params from assigned material"
        (test-util/with-prop [emitter-a :material (workspace/resolve-workspace-resource workspace "/materials/test_samplers.material")]
          (testing "emitter-a uses material"
            (let [sim-data (g/node-value emitter-a :emitter-sim-data)]
              (is (= (get-in sim-data [:shader])
                     (g/node-value material-node :shader)))
              (is (= (get-in sim-data [:gpu-texture :params])
                     (material/sampler->tex-params  (first (g/node-value material-node :samplers)))))))
          (testing "emitter-b does not use material"
            (let [sim-data (g/node-value emitter-b :emitter-sim-data)]
              (is (not= (get-in sim-data [:shader])
                        (g/node-value material-node :shader)))
              (is (not= (get-in sim-data [:gpu-texture :params])
                        (material/sampler->tex-params  (first (g/node-value material-node :samplers))))))))))))
