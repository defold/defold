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

(ns editor.gl.scratch
  (:require [editor.buffers :as buffers]
            [editor.gl.attribute :as gl.attribute]
            [util.coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private attribute-buffers-atom (atom {}))
(defonce ^:private index-buffers-atom (atom {}))

(defn attribute-buffer
  [element-types usage update-data-fn & args]
  (let [key (pair element-types usage)]
    (get
      (swap! attribute-buffers-atom
             update key
             (fn [attribute-buffer-lifecycle]
               (apply gl.attribute/update-attribute-buffer
                      (or attribute-buffer-lifecycle
                          (let [data (buffers/new-byte-buffer 0 :byte-order/native)
                                buffer-data (buffers/make-buffer-data data 0)]
                            (gl.attribute/make-attribute-buffer key buffer-data element-types usage)))
                      update-data-fn
                      args)))
      key)))

(defn index-buffer
  [data-type usage update-data-fn & args]
  (let [key (pair data-type usage)]
    (get
      (swap! index-buffers-atom
             update key
             (fn [index-buffer-lifecycle]
               (apply gl.attribute/update-index-buffer
                      (or index-buffer-lifecycle
                          (let [data (case data-type
                                       :type-unsigned-short
                                       (-> (buffers/new-byte-buffer 0 :byte-order/native)
                                           (.asShortBuffer))

                                       :type-unsigned-int
                                       (-> (buffers/new-byte-buffer 0 :byte-order/native)
                                           (.asIntBuffer)))
                                buffer-data (buffers/make-buffer-data data 0)]
                            (gl.attribute/make-index-buffer key buffer-data usage)))
                      update-data-fn
                      args)))
      key)))
