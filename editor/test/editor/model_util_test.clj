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

(ns editor.model-util-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as buffers]
            [editor.gl.attribute :as attribute]
            [editor.model-util :as model-util])
  (:import [editor.gl.attribute AttributeBufferBinding AttributeRenderArgBinding AttributeValueBinding]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-attribute-info
  [name-key semantic-type vector-type step-function location]
  {:name (name name-key)
   :name-key name-key
   :semantic-type semantic-type
   :vector-type vector-type
   :data-type :type-float
   :normalize false
   :coordinate-space :coordinate-space-local
   :step-function step-function
   :location location
   :attribute-transform :attribute-transform-none})

(defn- make-attribute-buffer
  [scene-node-id name-key vector-type values]
  (attribute/make-attribute-buffer
    {:request-type :model-util-test/attribute-buffer
     :scene-node-id scene-node-id
     :name-key name-key}
    (buffers/make-buffer-data (buffers/wrap-float-array (float-array values)))
    vector-type
    :static))

(deftest make-model-attribute-bindings-splits-by-step-function
  (let [scene-node-id 1
        position-info (make-attribute-info :position :semantic-type-position :vector-type-vec3 :vertex-step-function-vertex 0)
        color-info (make-attribute-info :color :semantic-type-color :vector-type-vec4 :vertex-step-function-instance 1)
        world-info (make-attribute-info :mtx-world :semantic-type-world-matrix :vector-type-mat4 :vertex-step-function-instance 2)
        position-buffer (make-attribute-buffer scene-node-id :position :vector-type-vec3 [0.0 0.0 0.0])
        color-buffer (make-attribute-buffer scene-node-id :color :vector-type-vec4 [1.0 0.0 0.0 1.0])
        bindings (model-util/make-model-attribute-bindings
                   scene-node-id
                   [position-info color-info world-info]
                   {:semantic-type-position [position-buffer]
                    :semantic-type-color [color-buffer]}
                   {})]

    (is (= [:color :mtx-world]
           (mapv :name-key (:instance-attribute-infos bindings))))
    (is (:has-instance-attributes? bindings))
    (is (instance? AttributeBufferBinding
                   (get-in bindings [:vertex-attribute-bindings :position])))
    (is (instance? AttributeValueBinding
                   (get-in bindings [:instance-attribute-bindings :color]))
        "Instance-step color should not use the per-vertex color mesh buffer.")
    (is (instance? AttributeRenderArgBinding
                   (get-in bindings [:instance-attribute-bindings :mtx-world])))
    (is (= #{:position :color :mtx-world}
           (set (keys (:attribute-bindings bindings)))))))

(deftest make-model-attribute-bindings-auto-promotes-matrix-semantics-to-instance-step
  (let [scene-node-id 1
        position-info (make-attribute-info :position :semantic-type-position :vector-type-vec3 :vertex-step-function-vertex 0)
        world-info (make-attribute-info :mtx-world :semantic-type-world-matrix :vector-type-mat4 :vertex-step-function-vertex 1)
        normal-info (make-attribute-info :mtx-normal :semantic-type-normal-matrix :vector-type-mat4 :vertex-step-function-vertex 5)
        animation-data-info (make-attribute-info :animation-data :semantic-type-none :vector-type-vec4 :vertex-step-function-vertex 9)
        position-buffer (make-attribute-buffer scene-node-id :position :vector-type-vec3 [0.0 0.0 0.0])
        bindings (model-util/make-model-attribute-bindings
                   scene-node-id
                   [position-info world-info normal-info animation-data-info]
                   {:semantic-type-position [position-buffer]}
                   {})]

    (is (= [:mtx-world :mtx-normal :animation-data]
           (mapv :name-key (:instance-attribute-infos bindings))))
    (is (= [:position]
           (keys (:vertex-attribute-bindings bindings))))
    (is (instance? AttributeRenderArgBinding
                   (get-in bindings [:instance-attribute-bindings :mtx-world])))
    (is (instance? AttributeRenderArgBinding
                   (get-in bindings [:instance-attribute-bindings :mtx-normal])))
    (is (instance? AttributeValueBinding
                   (get-in bindings [:instance-attribute-bindings :animation-data])))))
