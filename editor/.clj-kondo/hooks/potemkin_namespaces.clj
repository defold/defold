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

(ns hooks.potemkin-namespaces
  (:require [clj-kondo.hooks-api :as api]))

(defn- imported-symbols [form]
  (if (sequential? form)
    (let [[namespace-sym & imported-forms] form]
      (mapcat
        (fn [imported-form]
          (map
            (fn [imported-sym]
              (symbol
                (str namespace-sym
                     (when-let [imported-namespace (namespace imported-sym)]
                       (str "." imported-namespace)))
                (name imported-sym)))
            (imported-symbols imported-form)))
        imported-forms))
    [form]))

(defn- def-node [imported-sym]
  (api/list-node
    [(api/token-node 'def)
     (api/token-node (symbol (name imported-sym)))
     (api/token-node imported-sym)]))

(defn- imported-name [source-node name-node]
  (if name-node
    name-node
    (api/token-node (symbol (name (api/sexpr source-node))))))

(defn- preserve-source-node [source-node]
  (api/list-node
    [(api/token-node 'identity)
     source-node]))

(defn import-def [{:keys [node]}]
  (let [[_ source-node name-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (preserve-source-node source-node)
        (api/list-node
          [(api/token-node 'def)
           (imported-name source-node name-node)
           (api/token-node nil)])])}))

(defn import-fn [{:keys [node]}]
  (let [[_ source-node name-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (preserve-source-node source-node)
        (api/list-node
          [(api/token-node 'defn)
           (imported-name source-node name-node)
           (api/vector-node [(api/token-node '&)
                             (api/token-node (gensym "_args__"))])
           (api/token-node nil)])])}))

(defn import-macro [{:keys [node]}]
  (let [[_ source-node name-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (preserve-source-node source-node)
        (api/list-node
          [(api/token-node 'defmacro)
           (imported-name source-node name-node)
           (api/vector-node [(api/token-node '&)
                             (api/token-node (gensym "_args__"))])
           (api/token-node nil)])])}))

(defn import-vars [{:keys [node]}]
  (let [[_ & import-nodes] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'do)
         (mapcat
           (fn [import-node]
             (map def-node (imported-symbols (api/sexpr import-node))))
           import-nodes)))}))
