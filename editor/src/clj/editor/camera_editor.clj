;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.camera-editor
  (:require [clojure.string :as s]
            [plumbing.core :as pc]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-shapes :as scene-shapes]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Camera$CameraDesc]
           [javax.vecmath Matrix4d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(def camera-icon "icons/32/Icons_20-Camera.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom]
  {:form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :navigation false
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
                         :type :boolean}
                        {:path [:orthographic-projection]
                         :label "Orthographic Projection"
                         :type :boolean}
                        {:path [:orthographic-zoom]
                         :label "Orthographic Zoom"
                         :type :number}]}]
   :values {[:aspect-ratio] aspect-ratio
            [:fov] fov
            [:near-z] near-z
            [:far-z] far-z
            [:auto-aspect-ratio] auto-aspect-ratio
            [:orthographic-projection] orthographic-projection
            [:orthographic-zoom] orthographic-zoom}})

(g/defnk produce-pb-msg
  [aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom]
  {:aspect-ratio aspect-ratio
   :fov fov
   :near-z near-z
   :far-z far-z
   :auto-aspect-ratio (if (true? auto-aspect-ratio) 1 0)
   :orthographic-projection (if (true? orthographic-projection) 1 0)
   :orthographic-zoom orthographic-zoom})

(defn build-camera
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Camera$CameraDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-camera
      :user-data {:pb-msg pb-msg}})])

(defn unify-scale [renderable]
  (let [{:keys [^Quat4d world-rotation
                ^Vector3d world-scale
                ^Vector3d world-translation]} renderable
        min-scale (min (.-x world-scale) (.-y world-scale) (.-z world-scale))
        physics-world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setScale min-scale)
                                  (.setTranslation world-translation)
                                  (.setRotation world-rotation))]
    (assoc renderable :world-transform physics-world-transform)))

(defn wrap-uniform-scale [render-fn]
  (fn [gl render-args renderables n]
    (render-fn gl render-args (map unify-scale renderables) n)))

(def ^:private render-lines-uniform-scale (wrap-uniform-scale scene-shapes/render-lines))

(g/defnk produce-camera-scene
  [_node-id]
  (let [ext-x (* 0.5 2.0)
        ext-y (* 0.5 2.0)
        ext-z (* 0.5 2.0)
        ext [ext-x ext-y ext-z]
        neg-ext [(- ext-x) (- ext-y) (- ext-z)]
        aabb (geom/coords->aabb ext neg-ext)
        point-scale (float-array ext)
        color [1.0 1.0 1.0 1.0]]
    {:node-id _node-id
     :aabb aabb
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :tags #{:collision-shape :outline}
                              :passes [pass/outline]
                              :user-data (cond-> {:color color
                                                  :point-scale point-scale
                                                  :geometry scene-shapes/box-lines}

                                                 (zero? ext-z)
                                                 (assoc :point-count 8))}}]}))

(defn load-camera [project self resource camera]
  (g/set-property self
    :aspect-ratio (:aspect-ratio camera)
    :fov (:fov camera)
    :near-z (:near-z camera)
    :far-z (:far-z camera)
    :auto-aspect-ratio (not (zero? (:auto-aspect-ratio camera)))
    :orthographic-projection (not (zero? (:orthographic-projection camera)))
    :orthographic-zoom (:orthographic-zoom camera)))

(g/defnode CameraNode
  (inherits resource-node/ResourceNode)

  (property aspect-ratio g/Num)
  (property fov g/Num)
  (property near-z g/Num)
  (property far-z g/Num)
  (property auto-aspect-ratio g/Bool)
  (property orthographic-projection g/Bool)
  (property orthographic-zoom g/Num)

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :node-outline-key "Camera"
                                                      :label "Camera"
                                                      :icon camera-icon}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output scene g/Any produce-camera-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "camera"
                                    :node-type CameraNode
                                    :ddf-type Camera$CameraDesc
                                    :load-fn load-camera
                                    :icon camera-icon
                                    :view-types [:cljfx-form-view :text]
                                    :view-opts {}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Camera"))
