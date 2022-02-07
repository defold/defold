;; Copyright 2020 The Defold Foundation
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

(ns editor.scene-picking)

(defn picking-id->color [^long picking-id]
  (assert (<= picking-id 0xffffff))
  (let [red (bit-shift-right (bit-and picking-id 0xff0000) 16)
        green (bit-shift-right (bit-and picking-id 0xff00) 8)
        blue (bit-and picking-id 0xff)
        alpha 1.0]
    [(/ red 255.0) (/ green 255.0) (/ blue 255.0) alpha]))

(defn argb->picking-id [^long argb]
  (bit-and argb 0x00ffffff))

(defn renderable-picking-id-uniform [renderable]
  (assert (some? (:picking-id renderable)))
  (let [picking-id (:picking-id renderable)
        id-color (picking-id->color picking-id)]
    (float-array id-color)))
