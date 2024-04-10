;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.graph-util
  (:require [dynamo.graph :as g]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn array-subst-remove-errors [arr]
  (vec (remove g/error? arr)))

(defn explicit-outputs
  ([node-id]
   (explicit-outputs (g/now) node-id))
  ([basis node-id]
   ;; don't include arcs from override-original nodes
   (mapv (fn [[_ src-label tgt-id tgt-label]]
           [src-label [tgt-id tgt-label]])
         (g/explicit-outputs basis node-id))))

(defn- parse-load-properties-source-expression [source-expression]
  (cond
    (keyword? source-expression)
    (let [map-key source-expression]
      {:map-key map-key})

    (coll/list-or-cons? source-expression)
    (case (coll/bounded-count 4 source-expression)
      2 (let [[map-value->property-value sub-source-expression] source-expression]
          (assert (symbol? map-value->property-value))
          (assoc (parse-load-properties-source-expression sub-source-expression)
            :map-value->property-value map-value->property-value))
      3 (let [[map-key or-keyword default-expression] source-expression]
          (assert (keyword? map-key))
          (assert (= :or or-keyword))
          {:map-key map-key
           :default-expression default-expression}))

    :else
    (assert false)))

(defmacro set-properties-from-pb-map [node-id pb-class pb-map & mappings]
  (assert (symbol? node-id))
  (assert (symbol? pb-class))
  (assert (symbol? pb-map))
  (assert (seq mappings))
  (assert (even? (count mappings)))
  (cons `concat
        (map (fn [[property-symbol source-expression]]
               (assert (symbol? property-symbol))
               (let [property-label (keyword property-symbol)
                     {:keys [default-expression map-key map-value->property-value]} (parse-load-properties-source-expression source-expression)]
                 (cond
                   (and default-expression map-value->property-value)
                   `(g/set-property ~node-id ~property-label (~map-value->property-value (or (~map-key ~pb-map) ~default-expression)))

                   default-expression
                   `(g/set-property ~node-id ~property-label (or (~map-key ~pb-map) ~default-expression))

                   map-value->property-value
                   `(when-some [map-value# (~map-key ~pb-map)]
                      (g/set-property ~node-id ~property-label (~map-value->property-value map-value#)))

                   :else
                   `(when-some [map-value# (~map-key ~pb-map)]
                      (g/set-property ~node-id ~property-label map-value#)))))
             (partition 2 mappings))))
