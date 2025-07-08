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

(ns editor.gl.attribute
  (:require [editor.gl :as gl]
            [editor.gl.buffer :as gl.buffer]
            [editor.gl.protocols :refer [GlBind]]
            [util.defonce :as defonce]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record AttributeBufferBinding
  [attribute-buffer-lifecycle ^int base-location]

  GlBind
  (bind [_this gl render-args]
    (gl/bind gl attribute-buffer-lifecycle render-args)
    (gl.buffer/assign-attribute! attribute-buffer-lifecycle base-location))

  (unbind [_this gl render-args]
    (gl.buffer/clear-attribute! attribute-buffer-lifecycle gl base-location)
    (gl/unbind gl attribute-buffer-lifecycle render-args)))

(defn make-attribute-buffer-binding
  ^AttributeBufferBinding [attribute-buffer-lifecycle ^long base-location]
  (cond
    (not (gl.buffer/attribute-buffer? attribute-buffer-lifecycle))
    (throw (IllegalArgumentException. "attribute-buffer-lifecycle must be an attribute BufferLifecycle"))

    (not (nat-int? base-location))
    (throw (IllegalArgumentException. "base-location must be a non-negative integer"))

    :else
    (->AttributeBufferBinding attribute-buffer-lifecycle base-location)))
