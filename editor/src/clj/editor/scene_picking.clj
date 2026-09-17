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

(ns editor.scene-picking
  (:require [util.coll :as coll]
            [util.eduction :as e]))

(defn picking-id->color [^long picking-id]
  (assert (<= picking-id 0xffffff))
  (let [red (bit-shift-right (bit-and picking-id 0xff0000) 16)
        green (bit-shift-right (bit-and picking-id 0xff00) 8)
        blue (bit-and picking-id 0xff)
        alpha 1.0]
    (vector-of :float (/ red 255.0) (/ green 255.0) (/ blue 255.0) alpha)))

(defn picking-id->float-array
  ^floats [^long picking-id]
  (let [id-color (picking-id->color picking-id)]
    (float-array 4 id-color)))

(defn argb->picking-id [^long argb]
  (bit-and argb 0x00ffffff))

(defn renderable-picking-id-uniform
  ^floats [renderable]
  (picking-id->float-array (:picking-id renderable)))

(defn top-nodes
  [nodes]
  (let [node-ids (into #{} (map :node-id) nodes)
        child? (fn [node]
                 (->> (:node-id-path node)
                      (coll/some #(and (not= % (:node-id node))
                                       (contains? node-ids %)))))]
    (e/remove child? nodes)))
