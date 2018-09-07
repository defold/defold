(ns editor.display-profiles
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.math :as math]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [util.text-util :as text-util])
  (:import (com.dynamo.render.proto Render$DisplayProfiles)))

(set! *warn-on-reflection* true)

(def pb-def {:ext "display_profiles"
             :label "Display Profiles"
             :view-types [:form-view :text]
             :icon "icons/32/Icons_50-Display-profiles.png"
             :pb-class Render$DisplayProfiles})

(g/defnode ProfileNode
  (property name g/Str)
  (property qualifiers g/Any)

  (output form-values g/Any (g/fnk [_node-id name qualifiers]
                              {:node-id _node-id
                               :name name
                               :qualifiers qualifiers}))
  (output pb-msg g/Any (g/fnk [form-values]
                         (-> form-values
                             (dissoc :node-id)
                             (update :qualifiers
                                     (fn [qualifiers]
                                       (mapv #(update % :device-models text-util/parse-comma-separated-string)
                                             qualifiers))))))
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
  {:sections
   [{:title "Display Profiles"
     :fields [{:path [:profiles]
               :label "Profile"
               :type :2panel
               :panel-key {:path [:name] :type :string :default "New Display Profile"}
               :on-add (partial add-profile! _node-id "New Display Profile" [])
               :on-remove (fn [vals] (g/transact (map #(g/delete-node (:node-id %)) vals)))
               :set (fn [v path val] (g/set-property! (:node-id v) (first path) val))
               :panel-form {:sections
                            [{:fields [{:path [:qualifiers]
                                        :label "Qualifiers"
                                        :type :table
                                        :columns [{:path [:width]
                                                   :label "Width"
                                                   :type :integer}
                                                  {:path [:height]
                                                   :label "Height"
                                                   :type :integer}
                                                  {:path [:device-models]
                                                   :label "Device Models"
                                                   :type :string}]}]}]}}]}]})

(g/defnk produce-form-data [_node-id form-data-desc form-values]
  (assoc form-data-desc :values form-values))

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

(g/defnk produce-build-targets [_node-id resource pb-msg]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-pb
    :user-data {:pb pb-msg
                :pb-class (:pb-class pb-def)}
    :deps []}])

(g/defnode DisplayProfilesNode
  (inherits resource-node/ResourceNode)

  (input profile-msgs g/Any :array)
  (output pb-msg g/Any (g/fnk [profile-msgs] {:profiles profile-msgs}))
  (input profile-form-values g/Any :array)
  (output form-values g/Any (g/fnk [profile-form-values] {[:profiles] profile-form-values}))
  (output form-data-desc g/Any :cached produce-form-data-desc)
  (output form-data g/Any :cached produce-form-data)

  (input profile-data g/Any :array)
  (output profile-data g/Any (gu/passthrough profile-data))
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))

(defn load-display-profiles [project self resource pb]
  (for [p (:profiles pb)]
    (add-profile self (:name p) (:qualifiers p))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext (:ext pb-def)
    :label (:label pb-def)
    :build-ext (:build-ext pb-def)
    :node-type DisplayProfilesNode
    :ddf-type Render$DisplayProfiles
    :load-fn (fn [project self resource pb] (load-display-profiles project self resource pb))
    :icon (:icon pb-def)
    :view-types (:view-types pb-def)))
