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

(ns editor.gl.attribute-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as buffers]
            [editor.gl.attribute :as attribute]
            [editor.graphics.types :as graphics.types])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest interleaved-attribute-buffer-test
  (let [position-element-type (graphics.types/make-element-type :vector-type-vec3 :type-float false)
        texcoord-element-type (graphics.types/make-element-type :vector-type-vec2 :type-unsigned-short true)
        element-types [position-element-type texcoord-element-type]
        vertex-byte-size (graphics.types/element-types-byte-size element-types)
        buffer-data (buffers/make-buffer-data (buffers/new-byte-buffer (* 2 vertex-byte-size) :byte-order/native))
        attribute-buffer (attribute/make-attribute-buffer ::interleaved buffer-data element-types :static)
        texcoord-binding (attribute/make-attribute-buffer-binding attribute-buffer 1 7)]
    (is (= element-types (graphics.types/element-types attribute-buffer)))
    (is (= 2 (graphics.types/element-count attribute-buffer)))
    (is (= 7 (:base-location texcoord-binding)))
    (is (= [12] (:byte-offsets texcoord-binding)))))

(deftest attribute-buffer-rejects-partial-vertex-data-test
  (let [element-type (graphics.types/make-element-type :vector-type-vec3 :type-float false)
        buffer-data (buffers/make-buffer-data (buffers/new-byte-buffer 13 :byte-order/native))]
    (is (thrown-with-msg?
          ExceptionInfo
          #"buffer-data byte size must be divisible"
          (attribute/make-attribute-buffer ::partial buffer-data [element-type] :static)))))
