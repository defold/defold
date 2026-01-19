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

(ns editor.display-profiles
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [util.text-util :as text-util])
  (:import [com.dynamo.render.proto Render$DisplayProfile Render$DisplayProfileQualifier Render$DisplayProfiles]))

(set! *warn-on-reflection* true)

(def pb-def {:ext "display_profiles"
             :label (localization/message "resource.type.display-profiles")
             :view-types [:cljfx-form-view :text]
             :icon "icons/32/Icons_50-Display-profiles.png"
             :icon-class :property
             :pb-class Render$DisplayProfiles})

(def ^:private comma-separated-string->non-empty-vector
  (comp not-empty text-util/parse-comma-separated-string))

(defn- qualifier-form-value->pb [qualifier]
  (protobuf/make-map-without-defaults Render$DisplayProfileQualifier
    :width (:width qualifier)
    :height (:height qualifier)
    :device-models (comma-separated-string->non-empty-vector (:device-models qualifier))))

(g/defnk produce-profile-pb-msg [name qualifiers]
  (protobuf/make-map-without-defaults Render$DisplayProfile
    :name name
    :qualifiers (mapv qualifier-form-value->pb qualifiers)))

(g/defnode ProfileNode
  (property name g/Str) ; Required protobuf field.
  (property qualifiers g/Any) ; Vector always assigned in load-fn.

  (output form-values g/Any (g/fnk [_node-id name qualifiers]
                              {:node-id _node-id
                               :name name
                               :qualifiers qualifiers}))
  (output pb-msg g/Any produce-profile-pb-msg)
  (output profile-data g/Any (g/fnk [_node-id pb-msg]
                                    (assoc pb-msg :node-id _node-id))))

(defn- add-profile [node-id name qualifiers]
  (g/make-nodes (g/node-id->graph-id node-id)
                [profile [ProfileNode
                          :name name
                          :qualifiers (mapv #(update % :device-models text-util/join-comma-separated-string)
                                            qualifiers)]]
                (for [[from to] [[:_node-id :nodes]
                                 [:pb-msg :profile-msgs]
                                 [:form-values :profile-form-values]
                                 [:profile-data :profile-data]]]
                  (g/connect profile from node-id to))))

(defn- add-profile! [node-id name qualifiers]
  (g/transact (add-profile node-id name qualifiers)))

(g/defnk produce-form-data-desc [_node-id]
  {:navigation false
   :sections
   [{:localization-key "display-profiles"
     :fields [{:path [:auto-layout-selection]
               :localization-key "display-profiles.auto-layout-selection"
               :type :boolean}
              {:path [:profiles]
               :localization-key "display-profiles.profile"
               :type :2panel
               :panel-key {:path [:name] :type :string :default "New Display Profile"}
               :on-add #(add-profile! _node-id "New Display Profile" [])
               :on-remove (fn [vals] (g/transact (map #(g/delete-node (:node-id %)) vals)))
               :set (fn [v path val] (g/set-property! (:node-id v) (first path) val))
               :panel-form {:sections
                            [{:fields [{:path [:qualifiers]
                                        :localization-key "display-profiles.profile.qualifiers"
                                        :type :table
                                        :columns [{:path [:width]
                                                   :localization-key "display-profiles.profile.qualifiers.width"
                                                   :type :integer}
                                                  {:path [:height]
                                                   :localization-key "display-profiles.profile.qualifiers.height"
                                                   :type :integer}
                                                  {:path [:device-models]
                                                   :localization-key "display-profiles.profile.qualifiers.device-models"
                                                   :type :string}]}]}]}}]}]})

(g/defnk produce-form-data [_node-id form-data-desc form-values]
  (-> form-data-desc
      (assoc :values form-values)
      (assoc :form-ops {:user-data {:node-id _node-id}
                        :set protobuf-forms-util/set-form-op
                        :clear protobuf-forms-util/clear-form-op})))

(defn- build-pb [resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (resource/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class pb-def) pb)}))

(g/defnk produce-build-targets [_node-id resource save-value]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb save-value
                  :pb-class (:pb-class pb-def)}
      :deps []})])

(g/defnode DisplayProfilesNode
  (inherits resource-node/ResourceNode)
  (property auto-layout-selection g/Bool (default true)
            (dynamic visible (g/constantly false)))

  (input profile-msgs g/Any :array)
  (output save-value g/Any :cached (g/fnk [profile-msgs auto-layout-selection]
                                     (protobuf/make-map-without-defaults Render$DisplayProfiles
                                       :profiles profile-msgs
                                       :auto-layout-selection (boolean auto-layout-selection))))
  (input profile-form-values g/Any :array)
  (output form-values g/Any (g/fnk [profile-form-values auto-layout-selection]
                                   {[:profiles] profile-form-values
                                    [:auto-layout-selection] (boolean auto-layout-selection)}))
  (output form-data-desc g/Any :cached produce-form-data-desc)
  (output form-data g/Any :cached produce-form-data)

  (input profile-data g/Any :array)
  (output profile-data g/Any (gu/passthrough profile-data))
  (output build-targets g/Any :cached produce-build-targets))

(defn load-display-profiles [_project self _resource display-profiles]
  {:pre [(map? display-profiles)]} ; Render$DisplayProfiles in map format.
  ;; Inject any missing defaults into the stripped pb-map for form-view editing.
  (let [with-defaults (protobuf/inject-defaults Render$DisplayProfiles display-profiles)
        auto-layout (boolean (:auto-layout-selection with-defaults true))]
    (concat
      [(g/set-property self :auto-layout-selection auto-layout)]
      (for [display-profile (:profiles with-defaults)]
        (add-profile self (:name display-profile) (:qualifiers display-profile))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext (:ext pb-def)
    :label (:label pb-def)
    :build-ext (:build-ext pb-def)
    :node-type DisplayProfilesNode
    :ddf-type Render$DisplayProfiles
    :load-fn load-display-profiles
    :icon (:icon pb-def)
    :icon-class (:icon-class pb-def)
    :view-types (:view-types pb-def)))
