(ns editor.camera-editor
  (:require [clojure.string :as s]
            [plumbing.core :as pc]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.camera.proto Camera$CameraDesc]))

(set! *warn-on-reflection* true)

(def camera-icon "icons/32/Icons_20-Camera.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id aspect-ratio fov near-z far-z auto-aspect-ratio]
  {:form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :sections [{:title "Camera"
               :fields [{:path [:aspect-ratio]
                         :label "Aspect Ratio"
                         :type :number}
                        {:path [:fov]
                         :label "FOV"
                         :type :number}
                        {:path [:near-z]
                         :label "Near-Z"
                         :type :number}
                        {:path [:far-z]
                         :label "Far-Z"
                         :type :number}
                        {:path [:auto-aspect-ratio]
                         :label "Auto Aspect Ratio"
                         :type :boolean}]}]
   :values {[:aspect-ratio] aspect-ratio
            [:fov] fov
            [:near-z] near-z
            [:far-z] far-z
            [:auto-aspect-ratio] auto-aspect-ratio}})

(g/defnk produce-pb-msg
  [aspect-ratio fov near-z far-z auto-aspect-ratio]
  {:aspect-ratio aspect-ratio
   :fov fov
   :near-z near-z
   :far-z far-z
   :auto-aspect-ratio (if (true? auto-aspect-ratio) 1 0)})

(defn build-camera
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Camera$CameraDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-camera
    :user-data {:pb-msg pb-msg}}])

(defn load-camera [project self resource camera]
  (g/set-property self
    :aspect-ratio (:aspect-ratio camera)
    :fov (:fov camera)
    :near-z (:near-z camera)
    :far-z (:far-z camera)
    :auto-aspect-ratio (not (zero? (:auto-aspect-ratio camera)))))

(g/defnode CameraNode
  (inherits resource-node/ResourceNode)

  (property aspect-ratio g/Num)
  (property fov g/Num)
  (property near-z g/Num)
  (property far-z g/Num)
  (property auto-aspect-ratio g/Bool)

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :label "Camera"
                                                      :icon camera-icon}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "camera"
                                    :node-type CameraNode
                                    :ddf-type Camera$CameraDesc
                                    :load-fn load-camera
                                    :icon camera-icon
                                    :view-types [:form-view :text]
                                    :view-opts {}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Camera"))
