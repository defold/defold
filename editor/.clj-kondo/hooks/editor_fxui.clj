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

(ns hooks.editor-fxui
  (:require [clj-kondo.hooks-api :as api]))

(defn- compose-nodes [attr-map-node]
  (into []
        (comp
          (partition-all 2)
          (filter (fn [[key-node _value-node]]
                    (= :compose (api/sexpr key-node))))
          (mapcat (fn [[_key-node value-node]]
                    (:children value-node))))
        (:children attr-map-node)))

(defn- compose-node [attr-map-node]
  (api/list-node
    [(api/token-node 'let)
     (api/vector-node
       [(api/token-node 'props)
        (api/token-node nil)
        (api/token-node (gensym "_props__"))
        (api/token-node 'props)])
     (api/vector-node
       (compose-nodes attr-map-node))]))

(defn defc [{:keys [node]}]
  (let [[_ name-node attr-map-node & fn-tail] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (api/list-node
          (list*
            (api/token-node 'defn)
            name-node
            fn-tail))
        (compose-node attr-map-node)])}))
