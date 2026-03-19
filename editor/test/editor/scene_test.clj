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

(ns editor.scene-test
  (:require [clojure.test :refer :all]
            [editor.camera :as camera]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.math :as math]
            [editor.scene :as scene]
            [util.coll :refer [pair]])
  (:import [javax.vecmath Matrix4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- flatten-scene [scene preview-overrides]
  (#'scene/flatten-scene scene preview-overrides #{} #{} #{} math/identity-mat4))

(deftest displayed-node-properties-test
  (let [selected-node-properties
        [{:node-id 1
          :properties {:position {:value [0.0 0.0 0.0]
                                  :original-value [9.0 9.0 9.0]}
                       :rotation {:value [0.0 0.0 0.0 1.0]}}}
         {:node-id 2
          :properties {:position {:value [10.0 0.0 0.0]}
                       :scale {:value [1.0 1.0 1.0]}}}]]

    (testing "Overlays preview values onto matching node-id/property pairs."
      (is (= [{:node-id 1
               :properties {:position {:value [1.0 2.0 3.0]
                                       :original-value [9.0 9.0 9.0]}
                            :rotation {:value [0.0 0.0 0.0 1.0]}}}
              {:node-id 2
               :properties {:position {:value [20.0 0.0 0.0]}
                            :scale {:value [3.0 3.0 3.0]}}}]
             (scene/displayed-node-properties
               selected-node-properties
               {1 {:position [1.0 2.0 3.0]}
                2 {:position [20.0 0.0 0.0]
                   :scale [3.0 3.0 3.0]}}))))

    (testing "Ignores unmatched preview entries."
      (is (= selected-node-properties
             (scene/displayed-node-properties
               selected-node-properties
               {3 {:position [1.0 2.0 3.0]}
                1 {:scale [2.0 2.0 2.0]}}))))

    (testing "Returns the original input when there are no preview overrides."
      (is (identical? selected-node-properties
                      (scene/displayed-node-properties
                        selected-node-properties
                        nil)))
      (is (identical? selected-node-properties
                      (scene/displayed-node-properties
                        selected-node-properties
                        {}))))))

(deftest flatten-scene-renderables-same-node-id-preview-overrides-test
  (let [preview-fn-overrides-atom (atom [])

        preview-fn
        (fn preview-fn [local-aabb user-data prop-kw->override-value]
          (swap! preview-fn-overrides-atom conj prop-kw->override-value)
          [local-aabb user-data])

        scene
        {:node-id 0
         :renderable {:passes [pass/transparent]}
         :children [{:node-id 1
                     :renderable {:passes [pass/transparent]}
                     :children [{:node-id 1
                                 :renderable {:passes [pass/transparent]
                                              :preview-fn preview-fn}}]}]}

        preview-overrides
        {1 {:position [1.0 2.0 3.0]
            :custom :value}}

        renderables-by-pass (:renderables (flatten-scene scene preview-overrides))
        [_ parent-renderable child-renderable] (get renderables-by-pass pass/transparent)]

    (testing "Transform preview overrides are only applied once for repeated node ids."
      (is (= (Vector3d. 1.0 2.0 3.0) (:world-translation parent-renderable)))
      (is (= (Vector3d. 1.0 2.0 3.0) (:world-translation child-renderable))))

    (testing "Child :preview-fn still receives non-transform overrides."
      (is (= [{:custom :value}] @preview-fn-overrides-atom)))))

(deftest flatten-scene-produces-scene-aabb-test
  (let [scene {:node-id :root
               :children [{:node-id :child
                           :transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 10.0 0.0 5.0)))
                           :aabb (geom/coords->aabb [0.0 0.0 0.0] [2.0 2.0 2.0])}]}
        flattened-scene (flatten-scene scene nil)]
    (is (= (geom/coords->aabb [10.0 0.0 5.0] [12.0 2.0 7.0])
           (:scene-aabb flattened-scene)))))

(deftest flatten-scene-preview-overrides-affect-scene-aabb-test
  (let [scene {:node-id :root
               :children [{:node-id :child
                           :transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 1.0 2.0 3.0)))
                           :aabb (geom/coords->aabb [0.0 0.0 0.0] [2.0 2.0 2.0])}]}
        preview-overrides {:child {:position [10.0 20.0 30.0]}}
        flattened-scene (flatten-scene scene preview-overrides)]
    (is (= (geom/coords->aabb [10.0 20.0 30.0] [12.0 22.0 32.0])
           (:scene-aabb flattened-scene)))))

(deftest flatten-scene-preview-fn-uses-visibility-aabb-test
  (let [preview-args-atom (atom nil)
        preview-fn (fn preview-fn [visibility-aabb user-data prop-kw->override-value]
                     (reset! preview-args-atom [visibility-aabb user-data prop-kw->override-value])
                     (pair (geom/coords->aabb [-10.0 -20.0 -30.0] [40.0 50.0 60.0]) user-data))
        scene {:node-id :root
               :children [{:node-id :child
                           :visibility-aabb (geom/coords->aabb [-1.0 -2.0 -3.0] [4.0 5.0 6.0])
                           :aabb (geom/coords->aabb [100.0 100.0 100.0] [200.0 200.0 200.0])
                           :renderable {:preview-fn preview-fn}}]}
        flattened-scene (flatten-scene scene {:child {:custom :value}})]
    (is (= [(geom/coords->aabb [-1.0 -2.0 -3.0] [4.0 5.0 6.0]) nil {:custom :value}]
           @preview-args-atom))
    (is (= (geom/coords->aabb [-10.0 -20.0 -30.0] [40.0 50.0 60.0])
           (:scene-aabb flattened-scene)))))

(deftest flatten-scene-prefers-visibility-aabb-test
  (let [scene {:node-id :root
               :children [{:node-id :child
                           :visibility-aabb (geom/coords->aabb [-1.0 -2.0 -3.0] [4.0 5.0 6.0])
                           :aabb (geom/coords->aabb [100.0 100.0 100.0] [200.0 200.0 200.0])}]}
        flattened-scene (flatten-scene scene nil)]
    (is (= (geom/coords->aabb [-1.0 -2.0 -3.0] [4.0 5.0 6.0])
           (:scene-aabb flattened-scene)))))

(deftest flatten-scene-accumulates-all-sibling-subtrees-test
  (let [scene {:node-id :root
               :children [{:node-id :first
                           :aabb (geom/coords->aabb [-30.0 -80.0 -30.0] [30.0 80.0 30.0])
                           :renderable {:passes [pass/transparent]}}
                          {:node-id :second
                           :aabb (geom/coords->aabb [-32.0 -16.0 0.0] [32.0 16.0 0.0])
                           :renderable {:passes [pass/transparent]}}]}
        flattened-scene (flatten-scene scene nil)]
    (is (= (geom/coords->aabb [-32.0 -80.0 -30.0] [32.0 80.0 30.0])
           (:scene-aabb flattened-scene)))
    (is (= [:first :second]
           (mapv :node-id (get-in flattened-scene [:renderables pass/transparent]))))))

(deftest view-depth-sort-does-not-depend-on-clip-planes-test
  (let [camera-near (assoc (camera/make-camera) :z-near 1.0 :z-far 10.0)
        camera-far (assoc (camera/make-camera) :z-near 100.0 :z-far 1000.0)
        scene {:node-id :root
               :children [{:node-id :near
                           :transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 10.0)))
                           :renderable {:passes [pass/transparent]}}
                          {:node-id :far
                           :transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 -10.0)))
                           :renderable {:passes [pass/transparent]}}]}
        renderables-near (:renderables (scene/produce-scene-render-data {:scene scene
                                                                         :selection []
                                                                         :hidden-renderable-tags #{}
                                                                         :hidden-node-outline-key-paths #{}
                                                                         :local-camera camera-near}))
        renderables-far (:renderables (scene/produce-scene-render-data {:scene scene
                                                                        :selection []
                                                                        :hidden-renderable-tags #{}
                                                                        :hidden-node-outline-key-paths #{}
                                                                        :local-camera camera-far}))]
    (is (= (mapv :node-id (get renderables-near pass/transparent))
           (mapv :node-id (get renderables-far pass/transparent))))))

(deftest view-depth-sort-orders-farther-renderables-first-test
  (let [camera (camera/make-camera)
        view-matrix (camera/camera-view-matrix camera)
        near-world-transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 100.0)))
        far-world-transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 -100.0)))
        scene {:node-id :root
               :children [{:node-id :near
                           :transform near-world-transform
                           :renderable {:passes [pass/transparent]}}
                          {:node-id :far
                           :transform far-world-transform
                           :renderable {:passes [pass/transparent]}}]}
        renderables (:renderables (scene/produce-scene-render-data {:scene scene
                                                                    :selection []
                                                                    :hidden-renderable-tags #{}
                                                                    :hidden-node-outline-key-paths #{}
                                                                    :local-camera camera}))
        renderables-by-node-id (into {} (map (juxt :node-id identity)) (get renderables pass/transparent))]
    (is (> (#'scene/z-distance view-matrix far-world-transform)
           (#'scene/z-distance view-matrix near-world-transform)))
    (is (<= (compare (:render-key (get renderables-by-node-id :far))
                     (:render-key (get renderables-by-node-id :near)))
            0))))
