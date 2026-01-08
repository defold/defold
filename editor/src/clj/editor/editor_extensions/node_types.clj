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

(ns editor.editor-extensions.node-types
  "Namespace for defining stable name aliases for editor node types"
  (:require [dynamo.graph :as g]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private state-atom
  (atom {:name->type {}
         :type->name {}}))

(defn register-node-type-name!
  "Register node type <-> editor script name mapping"
  [node-type name]
  {:pre [(string? name) (g/node-type? node-type)]}
  (swap! state-atom (fn [state]
                      (-> state
                          (update :name->type assoc name node-type)
                          (update :type->name assoc node-type name))))
  nil)

(defn ->name
  "Given a node type, returns its name or nil if not registered"
  [node-type]
  {:pre [(g/node-type? node-type)]}
  ((:type->name @state-atom) node-type))

(defn ->type
  "Returns a node type given its name, or nil if not registered"
  [name]
  {:pre [(string? name)]}
  ((:name->type @state-atom) name))
