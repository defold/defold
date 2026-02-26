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

(ns editor.editor-tab
  (:require [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.ui :as ui])
  (:import [javafx.scene.control Tab]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn view-type
  "Returns the map describing the view-type of the view associated with the
  editor tab, if any. See workspace/register-view-type."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (and (map? %)
                   (keyword? (:id %))))]}
  (ui/user-data editor-tab ::view-type))

(defn set-view-type!
  "Associate a map describing the view-type of the view with the editor tab. The
  supplied view-type should be either a view-type map registered with the
  workspace or nil. Returns nil. See workspace/register-view-type."
  [^Tab editor-tab view-type]
  {:pre [(instance? Tab editor-tab)
         (or (nil? view-type)
             (and (map? view-type)
                  (keyword? (:id view-type))))]}
  (ui/user-data! editor-tab ::view-type view-type))

(defn view-type-id
  "Returns the keyword identifying the view-type associated with the editor tab,
  if any. See workspace/register-view-type."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (keyword? %))]}
  (:id (view-type editor-tab)))

(defn view-node-id
  "Returns the node-id of the view node associated with the editor tab, if any."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (g/node-id? %))]}
  (ui/user-data editor-tab ::view-node-id))

(defn set-view-node-id!
  "Associate a view node-id with the editor tab. The supplied view-node-id
  should be either the node-id of a view node in the graph or nil. Returns nil."
  [^Tab editor-tab view-node-id]
  {:pre [(instance? Tab editor-tab)
         (or (nil? view-node-id)
             (g/node-id? view-node-id))]}
  (ui/user-data! editor-tab ::view-node-id view-node-id))

(defn resource-node-id
  "Returns the node-id of the ResourceNode associated with the editor tab, if
  any."
  [^Tab editor-tab evaluation-context]
  {:post [(or (nil? %)
              (and (g/node-id? %)
                   (g/node-instance? (:basis evaluation-context) resource/ResourceNode %)))]}
  (some-> (view-node-id editor-tab)
          (g/node-value :view-data evaluation-context)
          second
          :resource-node))

(defn resource
  "Returns the Resource associated with the editor tab, if any."
  [^Tab editor-tab evaluation-context]
  {:post [(or (nil? %)
              (resource/resource? %))]}
  (some-> (resource-node-id editor-tab evaluation-context)
          (g/node-value :resource evaluation-context)))

(defn prefs-data
  "Returns the preference data associated with the editor tab, if any."
  [^Tab editor-tab evaluation-context]
  (when-let [view-type-id (view-type-id editor-tab)]
    (when-let [resource (resource editor-tab evaluation-context)]
      (let [proj-path (resource/proj-path resource)]
        [proj-path
         view-type-id]))))
