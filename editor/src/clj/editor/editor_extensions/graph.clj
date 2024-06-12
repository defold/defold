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

(ns editor.editor-extensions.graph
  "Code for interacting with graph from editor scripts, i.e. get/set values"
  (:require [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.util :as code.util]
            [editor.defold-project :as project]
            [editor.editor-extensions.validation :as validation]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.types :as types]
            [util.fn :as fn])
  (:import [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

(defn node-id-or-path->node-id
  "Coerce a value that may be node id or proj-path to existing node id"
  [node-id-or-path project evaluation-context]
  (if (string? node-id-or-path)
    (let [node-id (project/get-resource-node project node-id-or-path evaluation-context)]
      (when (nil? node-id)
        (throw (LuaError. (str node-id-or-path " not found"))))
      node-id)
    node-id-or-path))

(defn node-id->type-keyword [node-id ec]
  (g/node-type-kw (:basis ec) node-id))

;; region get

(defn- ensuring-converter [spec]
  (fn [value _outline-property _project _evaluation-context]
    (validation/ensure spec value)))

(defn- resource-converter [node-id-or-path outline-property project evaluation-context]
  (validation/ensure (s/or :nothing #{""} :resource ::validation/node-id-or-path) node-id-or-path)
  (when-not (= node-id-or-path "")
    (let [node-id (node-id-or-path->node-id node-id-or-path project evaluation-context)
          resource (g/node-value node-id :resource evaluation-context)
          ext (:ext (properties/property-edit-type outline-property))]
      (when (seq ext)
        (validation/ensure (set ext) (resource/type-ext resource)))
      resource)))

(def ^:private edit-type-id->value-converter
  {g/Str {:to identity :from (ensuring-converter string?)}
   g/Bool {:to identity :from (ensuring-converter boolean?)}
   g/Num {:to identity :from (ensuring-converter number?)}
   g/Int {:to identity :from (ensuring-converter int?)}
   types/Vec2 {:to identity :from (ensuring-converter (s/tuple number? number?))}
   types/Vec3 {:to identity :from (ensuring-converter (s/tuple number? number? number?))}
   types/Vec4 {:to identity :from (ensuring-converter (s/tuple number? number? number? number?))}
   'editor.resource.Resource {:to resource/proj-path :from resource-converter}})

(defn- property->prop-kw [property]
  (if (string/starts-with? property "__")
    (keyword property)
    (keyword (string/replace property "_" "-"))))

(defn- outline-property [node-id property ec]
  (when (g/node-instance? (:basis ec) outline/OutlineNode node-id)
    (let [prop-kw (property->prop-kw property)
          outline-property (-> node-id
                               (g/node-value :_properties ec)
                               (get-in [:properties prop-kw]))]
      (when (and outline-property
                 (properties/visible? outline-property)
                 (edit-type-id->value-converter (properties/edit-type-id outline-property)))
        (cond-> outline-property
                (not (contains? outline-property :prop-kw))
                (assoc :prop-kw prop-kw))))))

(defmulti ext-get (fn [node-id property ec]
                    [(node-id->type-keyword node-id ec) property]))

(defmethod ext-get [:editor.code.resource/CodeEditorResourceNode "text"] [node-id _ ec]
  (string/join \newline (g/node-value node-id :lines ec)))

(defmethod ext-get [:editor.resource/ResourceNode "path"] [node-id _ ec]
  (resource/resource->proj-path (g/node-value node-id :resource ec)))

(defn ext-value-getter
  "Create 0-arg fn that produces node property value

  Returns nil if there is no getter for node-id+property

  Args:
    node-id               the node id
    property              string property name
    evaluation-context    used evaluation context"
  [node-id property evaluation-context]
  (if (fn/multi-responds? ext-get node-id property evaluation-context)
    #(ext-get node-id property evaluation-context)
    (when-let [outline-property (outline-property node-id property evaluation-context)]
      (when-let [to (-> outline-property
                        properties/edit-type-id
                        edit-type-id->value-converter
                        :to)]
        #(some-> (properties/value outline-property) to)))))

;; endregion

;; region set

(defmulti ext-setter
  "Returns a function that receives value and returns txs"
  (fn [node-id property evaluation-context]
    [(node-id->type-keyword node-id evaluation-context) property]))

(defmethod ext-setter [:editor.code.resource/CodeEditorResourceNode "text"]
  [node-id _ _]
  (fn [value]
    [(g/set-property node-id :modified-lines (code.util/split-lines value))
     (g/update-property node-id :invalidated-rows conj 0)
     (g/set-property node-id :cursor-ranges [#code/range[[0 0] [0 0]]])
     (g/set-property node-id :regions [])]))

(defn ext-value-setter
  "Create 1-arg fn from new value to transaction steps setting the property

  Returns nil if there is no setter for the node-id+property pair"
  [node-id property project evaluation-context]
  (if (fn/multi-responds? ext-setter node-id property evaluation-context)
    (ext-setter node-id property evaluation-context)
    (when-let [outline-property (outline-property node-id property evaluation-context)]
      (when-not (properties/read-only? outline-property)
        (when-let [from (-> outline-property
                            properties/edit-type-id
                            edit-type-id->value-converter
                            :from)]
          #(properties/set-value evaluation-context
                                 outline-property
                                 (from % outline-property project evaluation-context)))))))

;; endregion
