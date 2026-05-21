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

(ns hooks.editor-gl-pass
  (:require [clj-kondo.hooks-api :as api]))

(defn- render-pass-node [name-node selection-node model-transform-node depth-clipping-node]
  (api/list-node
    [(api/token-node 'RenderPass.)
     (api/token-node (str (api/sexpr name-node)))
     selection-node
     model-transform-node
     depth-clipping-node]))

(defn- pass-def-node [[name-node selection-node model-transform-node depth-clipping-node]]
  (api/list-node
    [(api/token-node 'def)
     name-node
     (render-pass-node name-node selection-node model-transform-node depth-clipping-node)]))

(defn- pass-vector-node [passes]
  (api/vector-node
    (mapv (comp api/token-node api/sexpr first) passes)))

(defn- pass-list-def-node [name passes]
  (api/list-node
    [(api/token-node 'def)
     (api/token-node name)
     (pass-vector-node passes)]))

(defn make-passes [{:keys [node]}]
  (let [[_ & body] (:children node)
        passes (partition 4 body)
        selection-passes (filter #(true? (api/sexpr (second %))) passes)
        render-passes (remove #(true? (api/sexpr (second %))) passes)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'do)
         (concat
           (map pass-def-node passes)
           [(pass-list-def-node 'all-passes passes)
            (pass-list-def-node 'selection-passes selection-passes)
            (pass-list-def-node 'render-passes render-passes)])))}))
