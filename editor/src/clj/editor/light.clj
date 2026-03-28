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

(ns editor.light
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :as code.util]
            [editor.localization :as localization]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.types :as t]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto DataProto$Data]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- build-light [build-resource _dep-resources user-data]
  (let [{:keys [lines]} user-data
        text (data/lines->string lines)]
    (try
      (let [pb (protobuf/str->pb DataProto$Data text)
            content (protobuf/pb->bytes pb)]
        {:resource build-resource
         :content content})
      (catch Throwable e
        (throw (ex-info (str "Failed to compile .light file: " (.getMessage e))
                        {:build-resource build-resource}
                        e))))))

(g/defnk produce-build-targets [_node-id resource lines]
  (let [build-resource (workspace/make-build-resource resource)]
    [(bt/with-content-hash
       {:node-id _node-id
        :resource build-resource
        :build-fn build-light
        :user-data {:lines lines}})]))

(defn- struct-fields [data-map]
  (let [raw (get-in data-map [:data :struct :fields])]
    (into {}
          (map (fn [[k v]]
                 [(if (keyword? k) (name k) k) v]))
          raw)))

(defn- ensure-data-struct [data-map]
  (update data-map :data (fn [d]
                           (let [d (or d {})]
                             (update d :struct (fn [s]
                                                 (let [s (or s {})]
                                                   (update s :fields (fn [f] (or f {}))))))))))

(defn- list-field-vec4 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- list-field-vec3 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- field-key-str [field-key]
  (if (string? field-key) field-key (name field-key)))

(defn- assoc-field-number [data-map field-key n]
  (let [k (field-key-str field-key)]
    (-> (ensure-data-struct data-map)
        (assoc-in [:data :struct :fields k] {:number (double n)}))))

(defn- assoc-field-vec4 [data-map field-key v]
  (let [k (field-key-str field-key)
        v4 (vec (take 4 (concat v (repeat 1.0))))]
    (-> (ensure-data-struct data-map)
        (assoc-in [:data :struct :fields k] (list-field-vec4 v4)))))

(defn- assoc-field-vec3 [data-map field-key v]
  (let [k (field-key-str field-key)
        v3 (vec (take 3 (concat v [0.0 0.0 -1.0])))]
    (-> (ensure-data-struct data-map)
        (assoc-in [:data :struct :fields k] (list-field-vec3 v3)))))

(defn- get-number [fields key default]
  (double (or (get-in fields [key :number]) default)))

(defn- get-vec4 [fields key]
  (let [vals (get-in fields [key :list :values])]
    (if (seq vals)
      (mapv #(double (or (:number %) 0.0)) vals)
      [1.0 1.0 1.0 1.0])))

(defn- get-vec3 [fields key]
  (let [vals (get-in fields [key :list :values])]
    (if (seq vals)
      (mapv #(double (or (:number %) 0.0)) vals)
      [0.0 0.0 -1.0])))

(defn- data-map->lines [data-map]
  (code.util/split-lines (protobuf/pb->str (protobuf/map->pb DataProto$Data data-map) true)))

(defn- lines->data-map [lines]
  (let [text (data/lines->string lines)]
    (protobuf/str->map-with-defaults DataProto$Data text)))

(defn- set-lines-tx [update-data-map-fn]
  (fn [_evaluation-context node-id _old new-value]
    (let [lines (g/node-value node-id :lines _evaluation-context)]
      (if (g/error-value? lines)
        (throw (ex-info "Cannot edit properties while the light source has errors." {:node-id node-id}))
        (let [data-map (lines->data-map lines)
              new-map (update-data-map-fn data-map new-value)]
          [(g/set-property node-id :modified-lines (data-map->lines new-map))])))))

(def ^:private light-display-order
  [:light/color
   :light/intensity
   :light/range
   :light/direction
   :light/inner-cone-angle
   :light/outer-cone-angle])

(g/defnk produce-light-properties [_node-id _declared-properties lines]
  (cond
    (g/error-value? _declared-properties)
    _declared-properties

    (g/error-value? lines)
    (g/->error _node-id :lines :fatal nil (localization/message "error.light.invalid-source"))

    :else
    (try
      (let [declared (or _declared-properties {:properties {} :display-order []})
            data-map (lines->data-map lines)
            fields (struct-fields data-map)]
        (-> declared
            (update :properties merge
                    {:light/color
                     {:node-id _node-id
                      :prop-kw :light/color
                      :label (properties/label-message :light :color)
                      :type t/Vec4
                      :value (get-vec4 fields "color")
                      :edit-type {:type t/Vec4
                                  :labels ["R" "G" "B" "A"]
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-vec4 m "color" v)))}}

                     :light/intensity
                     {:node-id _node-id
                      :prop-kw :light/intensity
                      :label (properties/label-message :light :intensity)
                      :type g/Num
                      :value (get-number fields "intensity" 1.0)
                      :edit-type {:type g/Num
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-number m "intensity" v)))}}

                     :light/range
                     {:node-id _node-id
                      :prop-kw :light/range
                      :label (properties/label-message :light :range)
                      :type g/Num
                      :value (get-number fields "range" 10.0)
                      :edit-type {:type g/Num
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-number m "range" v)))}}

                     :light/direction
                     {:node-id _node-id
                      :prop-kw :light/direction
                      :label (properties/label-message :light :direction)
                      :type t/Vec3
                      :value (get-vec3 fields "direction")
                      :edit-type {:type t/Vec3
                                  :labels ["X" "Y" "Z"]
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-vec3 m "direction" v)))}}

                     :light/inner-cone-angle
                     {:node-id _node-id
                      :prop-kw :light/inner-cone-angle
                      :label (properties/label-message :light :inner-cone-angle)
                      :type g/Num
                      :value (get-number fields "inner_cone_angle" 0.0)
                      :edit-type {:type g/Num
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-number m "inner_cone_angle" v)))}}

                     :light/outer-cone-angle
                     {:node-id _node-id
                      :prop-kw :light/outer-cone-angle
                      :label (properties/label-message :light :outer-cone-angle)
                      :type g/Num
                      :value (get-number fields "outer_cone_angle" 45.0)
                      :edit-type {:type g/Num
                                  :set-fn (set-lines-tx (fn [m v] (assoc-field-number m "outer_cone_angle" v)))}}})
            (update :display-order (fn [d]
                                     (vec (distinct (concat (or d []) light-display-order)))))))
      (catch Throwable _e
        (g/->error _node-id :lines :fatal nil (localization/message "error.light.invalid-source"))))))

(g/defnode LightFileNode
  (inherits r/CodeEditorResourceNode)

  (output build-targets g/Any :cached produce-build-targets)
  (output _properties g/Properties :cached produce-light-properties))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
    :ext "light"
    :node-type LightFileNode
    :label (localization/message "resource.type.light")
    :icon "icons/32/Icons_11-Script-general.png"
    :icon-class :property
    :category (localization/message "resource.category.components")
    :view-types [:code :default]
    :view-opts {}
    :lazy-loaded true
    :built-pb-class DataProto$Data
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))
