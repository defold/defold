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

(ns integration.particlefx-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.material :as material]
            [editor.particle-lib :as plib]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [javax.vecmath Matrix4d]))

(defn- dump-outline [outline]
  {:_self (type (:_self outline)) :children (map dump-outline (:children outline))})

(deftest basic
  (testing "Basic scene"
           (test-util/with-loaded-project
             (let [node-id (test-util/resource-node project "/particlefx/default.particlefx")
                   scene (g/node-value node-id :scene)]
               (is (= 1 (count (:children scene))))))))

(deftest modifiers
  (testing "Basic scene"
           (test-util/with-loaded-project
             (let [node-id (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
                   outline (g/node-value node-id :node-outline)]
               (is (= 4 (count (:children outline))))
               (let [mod-drag (get-in outline [:children 3 :node-id])
                     props (:properties (g/node-value mod-drag :_properties))]
                 (doseq [key [:magnitude :max-distance]]
                   (is (not= nil (get-in props [key :value])))))))))

(deftest simulation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/particlefx/default.particlefx")
          prototype-msg (g/node-value node-id :rt-pb-data)
          emitter-sim-data (g/node-value node-id :emitter-sim-data)
          fetch-anim-fn (fn [index] (get emitter-sim-data index))
          transforms [(doto (Matrix4d.) (.setIdentity))]
          sim (plib/make-sim 16 256 prototype-msg transforms)
          attribute-infos [{:name "position"
                            :name-key :position
                            :vector-type :vector-type-vec3
                            :data-type :type-float
                            :normalize false
                            :semantic-type :semantic-type-position
                            :coordinate-space :coordinate-space-world
                            :step-function :vertex-step-function-vertex}]
          vertex-description (graphics.types/make-vertex-description attribute-infos)]
      (testing "Sim sleeping"
               (is (plib/sleeping? sim))
               (plib/simulate sim 1/60 fetch-anim-fn transforms)
               (is (not (plib/sleeping? sim))))
      (testing "Stats"
               (let [sim (-> sim
                             (plib/simulate 1/60 fetch-anim-fn transforms)
                             (plib/simulate 1/60 fetch-anim-fn transforms))
                     _stats (do (plib/gen-emitter-vertex-data sim 0 [1.0 1.0 1.0 1.0] 32 vertex-description {})
                               (plib/stats sim))]
                 (is (< 0 (:particles (plib/stats sim))))))
      (testing "Rendering"
        (is (= 12 (:v-count (plib/render-emitter sim 0)))))
      (testing "Dispose"
               (plib/destroy-sim sim)))))

(deftest particlefx-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/particlefx/default.particlefx")
          emitter (:node-id (test-util/outline node-id [0]))]
      (is (nil? (test-util/prop-error emitter :tile-source)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.atlas")]]
        (test-util/with-prop [emitter :tile-source v]
          (is (g/error? (test-util/prop-error emitter :tile-source)))))
      (is (nil? (test-util/prop-error emitter :material)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
        (test-util/with-prop [emitter :material v]
          (is (g/error? (test-util/prop-error emitter :material)))
          (is (g/error-value? (g/node-value node-id :build-targets)))))
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

(deftest manip-scale-preserves-types
  (test-util/with-loaded-project
    (let [project-graph (g/node-id->graph-id project)
          particlefx-path "/particlefx/fireworks_big.particlefx"
          particlefx (project/get-resource-node project particlefx-path)
          [[emitter] _ [modifier]] (g/sources-of particlefx :child-scenes)
          check! (fn check! [node-id prop-kw]
                   (doseq [original-curve-spread
                           [(properties/->curve-spread [[(float 0.0) (float 1.0) (float 1.0) (float 0.0)]] (float 0.0))
                            (properties/->curve-spread [[(double 0.0) (double 1.0) (double 1.0) (double 0.0)]] (double 0.0))
                            (properties/->curve-spread [(vector-of :float 0.0 1.0 1.0 0.0)] (float 0.0))
                            (properties/->curve-spread [(vector-of :double 0.0 1.0 1.0 0.0)] (double 0.0))]]
                     (with-open [_ (test-util/make-graph-reverter project-graph)]
                       (g/set-property! node-id prop-kw original-curve-spread)
                       (test-util/manip-scale! node-id [2.0 2.0 2.0])
                       (let [modified-curve-spread (g/node-value node-id prop-kw)]
                         (is (not= original-curve-spread modified-curve-spread))
                         (test-util/ensure-number-type-preserving! original-curve-spread modified-curve-spread)))))]

      (testing "Emitter"
        (check! emitter :emitter-key-size-x)
        (check! emitter :emitter-key-size-y)
        (check! emitter :emitter-key-size-z))

      (testing "Modifier"
        (check! modifier :magnitude)))))
