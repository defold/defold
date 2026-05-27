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

(ns editor.model-scene-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as buffers]
            [editor.geom :as geom]
            [editor.graphics.types :as graphics.types]
            [editor.math :as math]
            [editor.model-scene :as model-scene])
  (:import [editor.buffers BufferData]
           [editor.gl.attribute AttributeBufferBinding]
           [java.nio ByteBuffer ByteOrder]
           [javax.vecmath Matrix4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-instance-attribute-info
  [name-key semantic-type vector-type location]
  {:name (name name-key)
   :name-key name-key
   :semantic-type semantic-type
   :vector-type vector-type
   :data-type :type-float
   :normalize false
   :coordinate-space :coordinate-space-local
   :step-function :vertex-step-function-instance
   :location location
   :attribute-transform :attribute-transform-none})

(defn- attribute-bytes [values]
  (let [bytes (byte-array (* Float/BYTES (count values)))
        byte-buffer (buffers/wrap-byte-array bytes :byte-order/native)]
    (buffers/put! byte-buffer 0 :float false values)
    bytes))

(defn- attribute-ubytes [values]
  (let [bytes (byte-array (count values))
        byte-buffer (buffers/wrap-byte-array bytes :byte-order/native)]
    (buffers/put! byte-buffer 0 :ubyte false values)
    bytes))

(defn- binding-floats [^AttributeBufferBinding binding]
  (let [attribute-buffer-lifecycle (.-attribute-buffer-lifecycle binding)
        buffer-data ^BufferData (graphics.types/buffer-data attribute-buffer-lifecycle)
        byte-buffer (doto (.asReadOnlyBuffer ^ByteBuffer (.-data buffer-data))
                      (.order (ByteOrder/nativeOrder)))
        float-buffer (.asFloatBuffer byte-buffer)
        floats (float-array (.remaining float-buffer))]
    (.get float-buffer floats)
    (vec floats)))

(defn- binding-ubytes [^AttributeBufferBinding binding]
  (let [attribute-buffer-lifecycle (.-attribute-buffer-lifecycle binding)
        buffer-data ^BufferData (graphics.types/buffer-data attribute-buffer-lifecycle)
        byte-buffer (.asReadOnlyBuffer ^ByteBuffer (.-data buffer-data))
        bytes (byte-array (.remaining byte-buffer))]
    (.get byte-buffer bytes)
    (mapv #(bit-and 0xff (long %)) bytes)))

(deftest make-instance-attribute-buffer-bindings-encodes-per-renderable-values
  (let [world-1 (doto (Matrix4d.)
                  (.setIdentity)
                  (.setTranslation (Vector3d. 2.0 3.0 4.0)))
        world-2 (doto (Matrix4d.)
                  (.setIdentity)
                  (.setTranslation (Vector3d. 5.0 6.0 7.0)))
        world-info (make-instance-attribute-info :mtx-world :semantic-type-world-matrix :vector-type-mat4 0)
        color-info (make-instance-attribute-info :color :semantic-type-color :vector-type-vec4 4)
        animation-data-info (make-instance-attribute-info :animation-data :semantic-type-none :vector-type-vec4 5)
        packed-color-info (assoc (make-instance-attribute-info :packed-color :semantic-type-color :vector-type-vec4 6)
                            :data-type :type-unsigned-byte
                            :normalize true)
        renderables [{:world-transform world-1
                      :user-data {:scene-node-id 1
                                  :vertex-attribute-bytes {:color (attribute-bytes [0.25 0.5 0.75 1.0])
                                                           :packed-color (attribute-ubytes [255 128 0 255])}
                                  :instance-attribute-infos [world-info color-info animation-data-info packed-color-info]}}
                     {:world-transform world-2
                      :user-data {:scene-node-id 1
                                  :vertex-attribute-bytes {:color (attribute-bytes [1.0 0.75 0.5 0.25])
                                                           :packed-color (attribute-ubytes [0 64 192 255])}
                                  :instance-attribute-infos [world-info color-info animation-data-info packed-color-info]}}]
        make-instance-attribute-buffer-bindings (ns-resolve 'editor.model-scene 'make-instance-attribute-buffer-bindings)
        bindings (make-instance-attribute-buffer-bindings
                   {:view math/identity-mat4
                    :pass {:name :opaque}}
                   renderables)]
    (is (= (vec (concat (geom/as-array world-1)
                        (geom/as-array world-2)))
           (binding-floats (:mtx-world bindings))))
    (is (= [0.25 0.5 0.75 1.0
            1.0 0.75 0.5 0.25]
           (binding-floats (:color bindings))))
    (is (= [0.0 0.0 0.0 0.0
            0.0 0.0 0.0 0.0]
           (binding-floats (:animation-data bindings))))
    (is (= [255 128 0 255
            0 64 192 255]
           (binding-ubytes (:packed-color bindings))))))

(deftest model-batch-key-requires-instanced-world-transform
  (let [world-info (make-instance-attribute-info :mtx-world :semantic-type-world-matrix :vector-type-mat4 0)
        color-info (make-instance-attribute-info :color :semantic-type-color :vector-type-vec4 4)
        base-user-data {:coordinate-space-info {:coordinate-space-local #{:semantic-type-position}}
                        :has-instance-attributes? true
                        :index-buffer :index-buffer
                        :instance-attribute-infos [world-info color-info]
                        :material-data []
                        :mesh-renderable-buffers :mesh-renderable-buffers
                        :shader :shader
                        :textures {}
                        :vertex-attribute-bindings {:position :position-binding}}
        model-batch-key (ns-resolve 'editor.model-scene 'model-batch-key)]
    (is (some? (model-batch-key base-user-data)))
    (is (nil? (model-batch-key (assoc base-user-data :instance-attribute-infos [color-info]))))
    (is (nil? (model-batch-key (assoc base-user-data :coordinate-space-info {:coordinate-space-world #{:semantic-type-position}}))))))
