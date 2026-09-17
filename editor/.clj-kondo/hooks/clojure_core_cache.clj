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

(ns hooks.clojure-core-cache
  (:require [clj-kondo.hooks-api :as api]))

(defn- binding-symbols [argv-node]
  (into []
        (comp
          (filter symbol?)
          (remove #(-> % name (.startsWith "_"))))
        (tree-seq coll? seq (api/sexpr argv-node))))

(defn- consume-bindings-node [argv-node body]
  (api/list-node
    (list*
      (api/token-node 'let)
      (api/vector-node
        [(api/token-node (gensym "_method_args__"))
         (api/vector-node
           (mapv api/token-node (binding-symbols argv-node)))])
      (if (seq body)
        body
        [(api/token-node nil)]))))

(defn- method-node [node]
  (let [[method-name-node argv-node & body] (:children node)]
    (if (vector? (api/sexpr argv-node))
      (api/list-node
        [method-name-node
         argv-node
         (consume-bindings-node argv-node body)])
      node)))

(defn- specific-node [node]
  (if (seq (:children node))
    (method-node node)
    node))

(defn defcache [{:keys [node]}]
  (let [[_ type-name-node fields-node & specifics] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'deftype)
         type-name-node
         fields-node
         (map specific-node specifics)))}))
