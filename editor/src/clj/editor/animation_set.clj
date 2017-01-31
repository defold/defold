(ns editor.animation-set
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.murmur :as murmur])
  (:import [clojure.lang ExceptionInfo]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$AnimationSetDesc]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private animation-set-icon "icons/32/Icons_24-AT-Animation.png")

(defn- animation-instance-desc-pb-msg [resource]
  {:animation (resource/resource->proj-path resource)})

(g/defnk produce-desc-pb-msg [animations]
  {:animations (mapv animation-instance-desc-pb-msg animations)})

(g/defnk produce-save-data [resource desc-pb-msg]
  {:resource resource
   :content (protobuf/map->str Rig$AnimationSetDesc desc-pb-msg)})

(defn- prefix-animation-id [prefix animation]
  (update animation :id (fn [id] (string/join "/" (filter not-empty [prefix id])))))

(defn- base-name [resource]
  (-> resource resource/proj-path FilenameUtils/getBaseName))

(defn- merge-animations [merged-animations animations resource]
  (let [prefix-animation-id (partial prefix-animation-id (base-name resource))]
    (into merged-animations (map prefix-animation-id) animations)))

(defn- merge-bone-list [merged-bone-list bone-list]
  (when-not (every? true? (map = merged-bone-list bone-list))
    (throw (ex-info "The bone lists are incompatible."
                    {:cause :incompatible-bone-list
                     :bone-list bone-list
                     :merged-bone-list merged-bone-list})))
  (if (> (count bone-list) (count merged-bone-list))
    bone-list
    merged-bone-list))

(g/defnk produce-animation-set [_node-id animation-resources animation-sets]
  (assert (= (count animation-resources) (count animation-sets)))
  (try
    (reduce (fn [merged-animation-set [resource animation-set]]
              (-> merged-animation-set
                  (update :animations merge-animations (:animations animation-set) resource)
                  (update :bone-list merge-bone-list (:bone-list animation-set))))
            {:animations []
             :bone-list []}
            (map vector animation-resources animation-sets))
    (catch ExceptionInfo e
      (if (= :incompatible-bone-list (:cause e))
        (g/->error _node-id :animations :fatal animation-resources "The bone hierarchies of the animations are incompatible.")
        (throw e)))))

(defn hash-animation-set-ids [animation-set]
  (update animation-set :animations (partial mapv #(update % :id murmur/hash64))))

(defn- build-animation-set [_self _basis resource _dep-resources user-data]
  (let [animation-set-with-hash-ids (hash-animation-set-ids (:animation-set user-data))]
    {:resource resource
     :content (protobuf/map->bytes Rig$AnimationSet animation-set-with-hash-ids)}))

(g/defnk produce-animation-set-build-target [_node-id resource animation-set]
  {:node-id _node-id
   :resource (workspace/make-build-resource resource)
   :build-fn build-animation-set
   :user-data {:animation-set animation-set}})

(g/defnode AnimationSetNode
  (inherits project/ResourceNode)

  (input animation-resources resource/Resource :array)
  (input animation-sets g/Any :array)

  (property animations resource/ResourceVec
            (value (gu/passthrough animation-resources))
            (set (fn [_basis self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :animation-resources]
                                      [:animation-set :animation-sets]]]
                     (concat
                       (for [old-resource old-value]
                         (project/disconnect-resource-node project old-resource self connections))
                       (for [new-resource new-value]
                         (project/connect-resource-node project new-resource self connections))))))
            (dynamic visible (g/constantly false)))

  (output desc-pb-msg g/Any :cached produce-desc-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output animation-set g/Any :cached produce-animation-set)
  (output animation-set-build-target g/Any :cached produce-animation-set-build-target))

(defn- load-animation-set [_project self resource]
  (let [proj-path->resource (partial workspace/resolve-resource resource)
        pb (protobuf/read-text Rig$AnimationSetDesc resource)
        animation-proj-paths (map :animation (:animations pb))
        animation-resources (mapv proj-path->resource animation-proj-paths)]
    (g/set-property self :animations animation-resources)))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "animationset"
                                    :icon animation-set-icon
                                    :label "Animation Set"
                                    :load-fn load-animation-set
                                    :node-type AnimationSetNode
                                    :textual? true
                                    :view-types [:text]))
