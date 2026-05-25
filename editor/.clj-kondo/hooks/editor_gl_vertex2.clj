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

(ns hooks.editor-gl-vertex2
  (:require [clj-kondo.hooks-api :as api]))

(defn- ctor-symbol [name-sym]
  (symbol (str "->" name-sym)))

(defn- put-symbol [name-sym]
  (symbol (str name-sym "-put!")))

(defn- function-node []
  (api/list-node
    [(api/token-node 'fn)
     (api/vector-node [(api/token-node '&) (api/token-node '_args__)])
     (api/token-node nil)]))

(defn defvertex [{:keys [node]}]
  (let [[_ name-node] (:children node)
        name-sym (api/sexpr name-node)
        nodes (cond-> [(api/token-node 'do)
                       (api/list-node
                         [(api/token-node 'def)
                          name-node
                          (api/token-node nil)])
                       (api/list-node
                         [(api/token-node 'def)
                          (api/token-node (ctor-symbol name-sym))
                          (function-node)])]
                (not (:no-put (meta name-sym)))
                (conj
                  (api/list-node
                    [(api/token-node 'def)
                     (api/token-node (put-symbol name-sym))
                     (function-node)])))]
    {:node
     (api/list-node nodes)}))
