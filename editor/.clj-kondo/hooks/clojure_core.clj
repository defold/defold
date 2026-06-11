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

(ns hooks.clojure-core
  (:refer-clojure :exclude [map])
  (:require [clj-kondo.hooks-api :as api]))

(defn- map-fn-node [map-node arity]
  (let [args (mapv #(api/token-node (symbol (str "_x" % "__"))) (range arity))]
    (api/list-node
      [(api/token-node 'fn)
       (api/vector-node args)
       (api/list-node
         (list* (api/token-node 'get)
                map-node
                args))])))

(defn map [{:keys [node]}]
  (let [[map-symbol-node f-node & coll-nodes] (:children node)
        f-arity (case (count coll-nodes)
                  0 1
                  1 1
                  2 2
                  nil)]
    {:node
     (if (and f-arity (= :map (:tag f-node)))
       (api/list-node
         (list* map-symbol-node
                (map-fn-node f-node f-arity)
                coll-nodes))
       node)}))
