;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.gl.types
  (:require [editor.graphics.types :as graphics.types]
            [util.defonce :as defonce])
  (:import [com.jogamp.opengl GL2]
           [editor.buffers BufferData]
           [editor.graphics.types ElementType]
           [java.nio Buffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/protocol GLBinding
  (bind! [this gl render-args] "Bind this object to the GPU context.")
  (unbind! [this gl render-args] "Unbind this object from the GPU context."))

(defn data-type-gl-type
  ^long [data-type]
  (case data-type
    :type-byte GL2/GL_BYTE
    :type-unsigned-byte GL2/GL_UNSIGNED_BYTE
    :type-short GL2/GL_SHORT
    :type-unsigned-short GL2/GL_UNSIGNED_SHORT
    :type-int GL2/GL_INT
    :type-unsigned-int GL2/GL_UNSIGNED_INT
    :type-float GL2/GL_FLOAT))

(defn element-type-gl-type
  ^long [^ElementType element-type]
  (data-type-gl-type (.-data-type element-type)))

(defn element-buffer-gl-type
  ^long [element-buffer]
  (-> element-buffer graphics.types/element-type element-type-gl-type))

(defn usage-gl-usage
  ^long [usage]
  (case usage
    :dynamic GL2/GL_DYNAMIC_DRAW
    :static GL2/GL_STATIC_DRAW))

(defn gl-compatible-buffer? [^Buffer value]
  (and (instance? Buffer value)
       (or (.isDirect value)
           (.hasArray value))))

(defn gl-compatible-buffer-data? [^BufferData value]
  (and (instance? BufferData value)
       (gl-compatible-buffer? (.-data value))))
