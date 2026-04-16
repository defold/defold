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

(defn- scene-renderables-by-pass [scene local-camera]
  (-> {:scene scene
       :selection []
       :hidden-renderable-tags #{}
       :hidden-node-outline-key-paths #{}
       :local-camera local-camera}
      (scene/produce-scene-render-data)
      (:renderables)))

(deftest claim-scene-test
  (let [scene {:node-id :scene-node-id
               :children [{:node-id :scene-node-id
                           :children [{:node-id :scene-node-id}]}
                          {:node-id :tree-node-id
                           :node-outline-key "tree-node-outline-key"
                           :children [{:node-id :apple-node-id
                                       :node-outline-key "apple-node-outline-key"}]}
                          {:node-id :house-node-id
                           :node-outline-key "house-node-outline-key"
                           :children [{:node-id :door-node-id
                                       :node-outline-key "door-node-outline-key"
                                       :children [{:node-id :door-handle-node-id
                                                   :node-outline-key "door-handle-node-outline-key"}]}]}]}]
    (is (= {:node-id :new-node-id
            :node-outline-key "new-node-outline-key"
            :children [{:node-id :new-node-id
                        :node-outline-key "new-node-outline-key"
                        :children [{:node-id :new-node-id
                                    :node-outline-key "new-node-outline-key"}]}
                       {:node-id :tree-node-id
                        :node-outline-key "tree-node-outline-key"
                        :picking-node-id :new-node-id
                        :children [{:node-id :apple-node-id
                                    :node-outline-key "apple-node-outline-key"
                                    :picking-node-id :new-node-id}]}
                       {:node-id :house-node-id
                        :node-outline-key "house-node-outline-key"
                        :picking-node-id :new-node-id
                        :children [{:node-id :door-node-id
                                    :node-outline-key "door-node-outline-key"
                                    :picking-node-id :new-node-id
                                    :children [{:node-id :door-handle-node-id
                                                :node-outline-key "door-handle-node-outline-key"
                                                :picking-node-id :new-node-id}]}]}]}
           (scene/claim-scene scene :new-node-id "new-node-outline-key")))))

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

(deftest flatten-scene-preview-overrides-test
  (let [scene {:node-id :root
               :children [{:node-id :child
                           :transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 1.0 2.0 3.0)))
                           :aabb (geom/coords->aabb [10.0 10.0 10.0] [100.0 100.0 100.0])}]}]

    (testing "Without preview overrides."
      (let [flattened-scene (flatten-scene scene nil)]
        (is (= (geom/coords->aabb [11.0 12.0 13.0] [101.0 102.0 103.0])
               (:scene-aabb flattened-scene)))))

    (testing "With preview overrides."
      (let [preview-overrides {:child {:position [2.0 3.0 4.0]}}
            flattened-scene (flatten-scene scene preview-overrides)]
        (is (= (geom/coords->aabb [12.0 13.0 14.0] [102.0 103.0 104.0])
               (:scene-aabb flattened-scene)))))))

(deftest flatten-scene-preview-fn-test
  (let [aabb (geom/coords->aabb [100.0 100.0 100.0] [200.0 200.0 200.0])
        visibility-aabb (geom/coords->aabb [-1.0 -2.0 -3.0] [4.0 5.0 6.0])
        preview-aabb (geom/coords->aabb [-10.0 -20.0 -30.0] [40.0 50.0 60.0])
        preview-args-atom (atom nil)
        preview-fn (fn preview-fn [visibility-aabb user-data prop-kw->override-value]
                     (reset! preview-args-atom [visibility-aabb user-data prop-kw->override-value])
                     (pair preview-aabb
                           (merge user-data prop-kw->override-value)))
        renderable-user-data {:user-data-key :user-data-value}
        scene {:node-id :root
               :children [{:node-id :child
                           :aabb aabb
                           :visibility-aabb visibility-aabb
                           :renderable {:passes [pass/transparent]
                                        :preview-fn preview-fn
                                        :user-data renderable-user-data}}]}
        preview-position [2.0 3.0 4.0]
        preview-translation (doto (Vector3d.) (math/clj->vecmath preview-position))
        preview-transform-matrix (doto (Matrix4d.) (.setIdentity) (.setTranslation preview-translation))
        preview-aabb-transformed (geom/aabb-transform preview-aabb preview-transform-matrix)
        preview-overrides {:child {:position preview-position
                                   :custom :custom-value}}
        flattened-scene (flatten-scene scene preview-overrides)
        renderables-by-pass (:renderables flattened-scene)
        flattened-child-renderable (get-in renderables-by-pass [pass/transparent 0])]
    (is (= [visibility-aabb renderable-user-data {:custom :custom-value}]
           @preview-args-atom))
    (is (= :child
           (:node-id flattened-child-renderable)))
    (is (= preview-translation
           (:world-translation flattened-child-renderable)))
    (is (= preview-translation
           (math/translation (:world-transform flattened-child-renderable))))
    (is (= preview-aabb-transformed
           (:scene-aabb flattened-scene)))
    (is (= {:user-data-key :user-data-value
            :custom :custom-value}
           (:user-data flattened-child-renderable)))))

(deftest flatten-scene-preview-overrides-same-node-id-test
  (let [preview-fn-overrides-atom (atom [])

        preview-fn
        (fn preview-fn [visibility-aabb user-data prop-kw->override-value]
          (swap! preview-fn-overrides-atom conj prop-kw->override-value)
          [visibility-aabb user-data])

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

(deftest renderables-sort-back-to-front-test
  (let [camera (camera/make-camera)
        near-world-transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 -1.0)))
        far-world-transform (doto (Matrix4d.) (.setIdentity) (.setTranslation (Vector3d. 0.0 0.0 -100.0)))
        scene {:node-id :root
               :children [{:node-id :near
                           :transform near-world-transform
                           :renderable {:passes [pass/transparent]}}
                          {:node-id :far
                           :transform far-world-transform
                           :renderable {:passes [pass/transparent]}}]}
        renderables-by-pass (scene-renderables-by-pass scene camera)]
    (is (= [:far :near]
           (->> (get renderables-by-pass pass/transparent)
                (scene/render-sort)
                (mapv :node-id))))))

(deftest invalid-fixed-orthographic-camera-inset-test
  (let [renderable {:user-data {:is-orthographic true
                                :orthographic-mode :ortho-mode-fixed
                                :orthographic-zoom 0.0
                                :display-width 1920.0
                                :display-height 1080.0}}]
    (is (false? (#'scene/valid-camera-inset-orthographic-zoom? renderable)))
    (is (nil? (#'scene/camera-inset-dimensions renderable)))
    (is (nil? (#'scene/make-camera-inset-camera renderable 480.0 270.0)))))
