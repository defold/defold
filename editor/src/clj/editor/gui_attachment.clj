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

(ns editor.gui-attachment
  (:require [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [util.eduction :as e]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- scene-input-node [basis scene-node input]
  (gt/source-id ((g/explicit-arcs-by-target basis scene-node input) 0)))

(defn scene-node->layers-node [basis scene-node]
  (scene-input-node basis scene-node :layers-node))

(defn scene-node->materials-node [basis scene-node]
  (scene-input-node basis scene-node :materials-node))

(defn scene-node->particlefx-resources-node [basis scene-node]
  (scene-input-node basis scene-node :particlefx-resources-node))

(defn scene-node->textures-node [basis scene-node]
  (scene-input-node basis scene-node :textures-node))

(defn scene-node->layouts-node [basis scene-node]
  (scene-input-node basis scene-node :layouts-node))

(defn scene-node->fonts-node [basis scene-node]
  (scene-input-node basis scene-node :fonts-node))

(defn scene-node->node-tree [basis scene-node]
  (scene-input-node basis scene-node :node-tree))

(defn next-child-index [parent-node evaluation-context]
  (->> (g/node-value parent-node :child-indices evaluation-context)
       (e/map second)
       (reduce max -1)
       long
       inc))
