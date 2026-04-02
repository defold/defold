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
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto DataProto$Data]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private light-icon "icons/32/Icons_21-Light.png")

;; Property inspector and form choicebox: [value label] pairs (see editor.properties-view/make-control-view :choicebox).
(def ^:private light-type-options
  [[:point "Point"]
   [:directional "Directional"]
   [:spot "Spot"]])

(defn- list-field-vec4 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- list-field-vec3 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- field-num [n]
  {:number (double n)})

(defn- struct-fields [data-map]
  (let [raw (get-in data-map [:data :struct :fields])]
    (into {}
          (map (fn [[k v]]
                 [(if (keyword? k) (name k) k) v]))
          raw)))

(defn- tags->light-type [tags]
  (cond
    (some #{"directional_light"} tags) :directional
    (some #{"spot_light"} tags) :spot
    :else :point))

(defn- light-type->tags [light-type]
  (case light-type
    :directional ["light" "directional_light"]
    :spot ["light" "spot_light"]
    ["light" "point_light"]))

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

(defn- parse-data-desc [light-desc]
  ;; GameObject$Data in map format.
  (let [tags (vec (:tags light-desc))
        light-type (tags->light-type tags)
        fields (struct-fields light-desc)]
    {:light-type light-type
     :color (get-vec4 fields "color")
     :intensity (get-number fields "intensity" 1.0)
     :range (get-number fields "range" 10.0)
     :direction (get-vec3 fields "direction")
     :inner-cone-angle (get-number fields "inner_cone_angle" 0.0)
     :outer-cone-angle (get-number fields "outer_cone_angle" 45.0)}))

(defn- build-data-desc
  [light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (let [tags (light-type->tags light-type)
        c4 (vec (take 4 (concat color (repeat 1.0))))
        fields (case light-type
                 :point {"color" (list-field-vec4 c4)
                         "intensity" (field-num intensity)
                         "range" (field-num range)}
                 :directional {"color" (list-field-vec4 c4)
                               "intensity" (field-num intensity)
                               "direction" (list-field-vec3 (vec (take 3 (concat direction [0.0 0.0 -1.0]))))}
                 :spot {"color" (list-field-vec4 c4)
                        "intensity" (field-num intensity)
                        "range" (field-num range)
                        "inner_cone_angle" (field-num inner-cone-angle)
                        "outer_cone_angle" (field-num outer-cone-angle)})]
    (protobuf/make-map-without-defaults DataProto$Data
      :tags tags
      :data {:struct {:fields fields}})))

(defn- build-light [build-resource _dep-resources user-data]
  (let [{:keys [pb-map]} user-data]
    {:resource build-resource
     :content (protobuf/map->bytes DataProto$Data pb-map)}))

(g/defnk produce-build-targets [_node-id resource save-value]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-light
      :user-data {:pb-map save-value}})])

(g/defnk produce-save-value
  [light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (build-data-desc light-type color intensity range direction inner-cone-angle outer-cone-angle))

(g/defnk produce-outline-data [_node-id]
  {:node-id _node-id
   :node-outline-key "Light"
   :label (localization/message "resource.type.light")
   :icon light-icon})

(defn- light-set-form-op [{:keys [node-id]} [prop] value]
  (case prop
    :direction (g/set-property node-id :direction (vec (take 3 value)))
    (protobuf-forms-util/set-form-op {:node-id node-id} [prop] value)))

(g/defnk produce-form-data
  [_node-id light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (let [direction-vec4 (vec (take 4 (concat direction [0.0 0.0 -1.0 0.0])))
        hidden-range (= :directional light-type)
        hidden-direction (not= :directional light-type)
        hidden-cones (not= :spot light-type)]
    {:navigation false
     :form-ops {:user-data {:node-id _node-id}
                :set light-set-form-op
                :clear protobuf-forms-util/clear-form-op}
     :sections [{:localization-key "light"
                 :fields [{:path [:light-type]
                           :localization-key "light.type"
                           :type :choicebox
                           :options light-type-options
                           :default :point}
                          {:path [:color]
                           :localization-key "light.color"
                           :type :vec4
                           :default [1.0 1.0 1.0 1.0]}
                          {:path [:intensity]
                           :localization-key "light.intensity"
                           :type :number
                           :default 1.0}
                          {:path [:range]
                           :localization-key "light.range"
                           :type :number
                           :default 10.0
                           :hidden hidden-range}
                          {:path [:direction]
                           :localization-key "light.direction"
                           :type :vec4
                           :default [0.0 0.0 -1.0 0.0]
                           :hidden hidden-direction}
                          {:path [:inner-cone-angle]
                           :localization-key "light.inner-cone-angle"
                           :type :number
                           :default 0.0
                           :hidden hidden-cones}
                          {:path [:outer-cone-angle]
                           :localization-key "light.outer-cone-angle"
                           :type :number
                           :default 45.0
                           :hidden hidden-cones}]}]
     :values {[:light-type] light-type
              [:color] color
              [:intensity] intensity
              [:range] range
              [:direction] direction-vec4
              [:inner-cone-angle] inner-cone-angle
              [:outer-cone-angle] outer-cone-angle}}))

(defn- sanitize-light [light-desc]
  {:pre [(map? light-desc)]}
  ;; Keep tags and data; strip empty optional protobuf cruft if needed later.
  light-desc)

(defn load-light [_project self _resource light-desc]
  {:pre [(map? light-desc)]} ; DataProto$Data in map format.
  (let [m (parse-data-desc light-desc)]
    (apply g/set-properties self (apply concat m))))

(g/defnode LightNode
  (inherits resource-node/ResourceNode)

  (property light-type g/Keyword (default :point)
            (dynamic label (properties/label-dynamic :light :type))
            (dynamic edit-type (g/constantly {:type :choicebox :options light-type-options})))
  (property color types/Vec4 (default [1.0 1.0 1.0 1.0])
            (dynamic label (properties/label-dynamic :light :color))
            (dynamic edit-type (g/constantly {:type types/Vec4 :labels ["R" "G" "B" "A"]})))
  (property intensity g/Num (default 1.0)
            (dynamic label (properties/label-dynamic :light :intensity)))
  (property range g/Num (default 10.0)
            (dynamic label (properties/label-dynamic :light :range))
            (dynamic visible (g/fnk [light-type] (contains? #{:point :spot} light-type))))
  (property direction types/Vec3 (default [0.0 0.0 -1.0])
            (dynamic label (properties/label-dynamic :light :direction))
            (dynamic edit-type (g/constantly {:type types/Vec3 :labels ["X" "Y" "Z"]}))
            (dynamic visible (g/fnk [light-type] (= :directional light-type))))
  (property inner-cone-angle g/Num (default 0.0)
            (dynamic label (properties/label-dynamic :light :inner-cone-angle))
            (dynamic visible (g/fnk [light-type] (= :spot light-type))))
  (property outer-cone-angle g/Num (default 45.0)
            (dynamic label (properties/label-dynamic :light :outer-cone-angle))
            (dynamic visible (g/fnk [light-type] (= :spot light-type))))

  (display-order [:light-type :color :intensity :range :direction :inner-cone-angle :outer-cone-angle])

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output node-outline outline/OutlineData :cached produce-outline-data))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "light"
    :node-type LightNode
    :ddf-type DataProto$Data
    :load-fn load-light
    :sanitize-fn sanitize-light
    :icon light-icon
    :icon-class :property
    :category (localization/message "resource.category.components")
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{}}}
    :label (localization/message "resource.type.light")))
