(ns integration.particlefx-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.collection :as collection]
            [editor.handler :as handler]
            [editor.defold-project :as project]
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
               (let [mod-drag (get-in outline [:children 2 :node-id])
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
