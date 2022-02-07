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

(ns editor.graph-util
  (:require [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn array-subst-remove-errors [arr]
  (vec (remove g/error? arr)))

(defn explicit-outputs [node]
  ;; don't include arcs from override-original nodes
  (mapv (fn [[_ src-label tgt-id tgt-label]] [src-label [tgt-id tgt-label]]) (g/explicit-outputs node)))
