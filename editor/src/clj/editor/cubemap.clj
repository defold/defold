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

(ns editor.cubemap
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.localization :as localization]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.texture-util :as texture-util]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap]
           [com.jogamp.opengl GL2]))

(set! *warn-on-reflection* true)

(def cubemap-icon "icons/32/Icons_23-Cubemap.png")

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

(def ^:private sphere-lats 32)
(def ^:private sphere-longs 64)

(def unit-sphere
  (let [vbuf (->normal-vtx (* 6 (* sphere-lats sphere-longs)))]
    (doseq [face (geom/unit-sphere-pos-nrm sphere-lats sphere-longs)
            v    face]
      (conj! vbuf v))
    (persistent! vbuf)))

(shader/defshader pos-norm-vert
  (uniform mat4 world)
  (attribute vec3 position)
  (attribute vec3 normal)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vNormal (normalize (* (mat3 (.xyz (nth world 0)) (.xyz (nth world 1)) (.xyz (nth world 2))) normal)))
    (setq vWorld (.xyz (* world (vec4 position 1.0))))
    (setq gl_Position (* gl_ModelViewProjectionMatrix (vec4 position 1.0)))))

(shader/defshader pos-norm-frag
  (uniform vec3 cameraPosition)
  (uniform samplerCube envMap)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vec3 camToV (normalize (- vWorld cameraPosition)))
    (setq vec3 refl (reflect camToV vNormal))
    (setq gl_FragColor (textureCube envMap refl))))

; TODO - macro of this
(def cubemap-shader (shader/make-shader ::cubemap-shader pos-norm-vert pos-norm-frag))

(defn render-cubemap
  [^GL2 gl render-args camera gpu-texture vertex-binding]
  (gl/with-gl-bindings gl render-args [cubemap-shader vertex-binding gpu-texture]
    (shader/set-uniform cubemap-shader gl "world" geom/Identity4d)
    (shader/set-uniform cubemap-shader gl "cameraPosition" (types/position camera))
    (shader/set-uniform cubemap-shader gl "envMap" 0)
    (gl/gl-enable gl GL2/GL_CULL_FACE)
    (gl/gl-cull-face gl GL2/GL_BACK)
    (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (* 6 (* sphere-lats sphere-longs)))
    (gl/gl-disable gl GL2/GL_CULL_FACE)))

(g/defnk produce-save-value [right left top bottom front back]
  (protobuf/make-map-without-defaults Graphics$Cubemap
    :right (resource/resource->proj-path right)
    :left (resource/resource->proj-path left)
    :top (resource/resource->proj-path top)
    :bottom (resource/resource->proj-path bottom)
    :front (resource/resource->proj-path front)
    :back (resource/resource->proj-path back)))

(def ^:private cubemap-aabb (geom/coords->aabb [1 1 1] [-1 -1 -1]))

(g/defnk produce-scene
  [_node-id gpu-texture]
  (let [vertex-binding (vtx/use-with _node-id unit-sphere cubemap-shader)]
    {:node-id    _node-id
     :aabb       cubemap-aabb
     :renderable {:render-fn (fn [gl render-args _renderables _count]
                               (let [camera (:camera render-args)]
                                 (render-cubemap gl render-args camera gpu-texture vertex-binding)))
                  :tags #{:cubemap}
                  :passes [pass/transparent]}}))

(defn- cubemap-side-setter [resource-label image-generator-label size-label]
  (fn [evaluation-context self old-value new-value]
    (project/resource-setter evaluation-context self old-value new-value
                             [:resource resource-label]
                             [:content-generator image-generator-label]
                             [:size size-label])))

(defn- generate-images [image-generators]
  (into {}
        (map (fn [[dir generator]]
               [dir ((:f generator) (:args generator))]))
        image-generators))

(defn- build-cubemap [resource dep-resources user-data]
  (let [{:keys [image-generators texture-profile compress?]} user-data
        images (generate-images image-generators)]
    (g/precluding-errors
      (vals images)
      (let [texture-images (tex-gen/make-cubemap-texture-images images texture-profile compress?)
            cubemap-texture-image-generate-result (tex-gen/assemble-cubemap-texture-images texture-images)]
        {:resource resource
         :write-content-fn tex-gen/write-texturec-content-fn
         :user-data {:texture-generator-result cubemap-texture-image-generate-result}}))))

(def ^:private cubemap-dir->property
  {:px :right
   :nx :left
   :py :top
   :ny :bottom
   :pz :front
   :nz :back})

(defn- cubemap-images-missing-error [node-id cubemap-image-resources]
  (when-let [error (some (fn [[dir image-resource]]
                           (when-let [message (validation/prop-resource-missing? image-resource (properties/keyword->name (cubemap-dir->property dir)))]
                             [dir message]))
                         cubemap-image-resources)]
    (let [[dir message] error]
      (g/->error node-id (cubemap-dir->property dir) :fatal nil message))))

(defn- size->width-x-height [size]
  (format "%sx%s" (:width size) (:height size)))

(defn- cubemap-image-sizes-error [node-id cubemap-image-sizes]
  (let [sizes (vals cubemap-image-sizes)]
    (when (and (every? some? sizes)
               (not (apply = sizes)))
      (let [message (apply format
                           "Cubemap image sizes differ:\n%s = %s\n%s = %s\n%s = %s\n%s = %s\n%s = %s\n%s = %s"
                           (mapcat (fn [dir] [(properties/keyword->name (cubemap-dir->property dir)) (size->width-x-height (dir cubemap-image-sizes))])
                                   [:px :nx :py :ny :pz :nz]))]
        (g/->error node-id nil :fatal nil message)))))

(g/defnk produce-build-targets [_node-id resource cubemap-image-generators cubemap-image-resources cubemap-image-sizes texture-profile build-settings]
  (g/precluding-errors
    [(cubemap-images-missing-error _node-id cubemap-image-resources)
     (cubemap-image-sizes-error _node-id cubemap-image-sizes)]
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-cubemap
        :user-data {:image-generators cubemap-image-generators
                    :texture-profile texture-profile
                    :compress? (:compress? build-settings false)}})]))

(defmacro cubemap-side-error [property]
  (let [prop-kw (keyword property)
        prop-name (properties/keyword->name prop-kw)]
    `(g/fnk [~'_node-id ~property ~'cubemap-image-sizes]
            (or (validation/prop-error :fatal ~'_node-id ~prop-kw validation/prop-resource-missing? ~property ~prop-name)
                (cubemap-image-sizes-error ~'_node-id ~'cubemap-image-sizes)))))

(g/defnode CubemapNode
  (inherits resource-node/ResourceNode)
  (inherits scene/SceneNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (property right resource/Resource                         ; Required protobuf field.
            (value (gu/passthrough right-resource))
            (set (cubemap-side-setter :right-resource :right-image-generator :right-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error right))
            (dynamic label (properties/label-dynamic :cubemap :right))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :right)))
  (property left resource/Resource                          ; Required protobuf field.
            (value (gu/passthrough left-resource))
            (set (cubemap-side-setter :left-resource :left-image-generator :left-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error left))
            (dynamic label (properties/label-dynamic :cubemap :left))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :left)))
  (property top resource/Resource                           ; Required protobuf field.
            (value (gu/passthrough top-resource))
            (set (cubemap-side-setter :top-resource :top-image-generator :top-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error top))
            (dynamic label (properties/label-dynamic :cubemap :top))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :top)))
  (property bottom resource/Resource                        ; Required protobuf field.
            (value (gu/passthrough bottom-resource))
            (set (cubemap-side-setter :bottom-resource :bottom-image-generator :bottom-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error bottom))
            (dynamic label (properties/label-dynamic :cubemap :bottom))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :bottom)))
  (property front resource/Resource                         ; Required protobuf field.
            (value (gu/passthrough front-resource))
            (set (cubemap-side-setter :front-resource :front-image-generator :front-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error front))
            (dynamic label (properties/label-dynamic :cubemap :front))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :front)))
  (property back resource/Resource                          ; Required protobuf field.
            (value (gu/passthrough back-resource))
            (set (cubemap-side-setter :back-resource :back-image-generator :back-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error back))
            (dynamic label (properties/label-dynamic :cubemap :back))
            (dynamic tooltip (properties/tooltip-dynamic :cubemap :back)))

  (input right-resource  resource/Resource)
  (input left-resource   resource/Resource)
  (input top-resource    resource/Resource)
  (input bottom-resource resource/Resource)
  (input front-resource  resource/Resource)
  (input back-resource   resource/Resource)

  (input right-image-generator  g/Any)
  (input left-image-generator   g/Any)
  (input top-image-generator    g/Any)
  (input bottom-image-generator g/Any)
  (input front-image-generator  g/Any)
  (input back-image-generator   g/Any)

  (input right-size  g/Any)
  (input left-size   g/Any)
  (input top-size    g/Any)
  (input bottom-size g/Any)
  (input front-size  g/Any)
  (input back-size   g/Any)

  (output cubemap-image-resources g/Any
          (g/fnk [right-resource left-resource top-resource bottom-resource front-resource back-resource]
            {:px right-resource :nx left-resource :py top-resource :ny bottom-resource :pz front-resource :nz back-resource}))

  (output cubemap-image-generators g/Any
          (g/fnk [right-image-generator left-image-generator top-image-generator bottom-image-generator front-image-generator back-image-generator]
            {:px right-image-generator :nx left-image-generator :py top-image-generator :ny bottom-image-generator :pz front-image-generator :nz back-image-generator}))

  (output cubemap-image-sizes g/Any
          (g/fnk [right-size left-size top-size bottom-size front-size back-size]
            {:px right-size :nx left-size :py top-size :ny bottom-size :pz front-size :nz back-size}))

  (output gpu-texture-generator g/Any :cached
          (g/fnk [_node-id cubemap-image-resources cubemap-image-generators cubemap-image-sizes texture-profile]
            (g/precluding-errors
              [(cubemap-images-missing-error _node-id cubemap-image-resources)
               (cubemap-image-sizes-error _node-id cubemap-image-sizes)]
              (texture-util/make-cubemap-gpu-texture-generator _node-id cubemap-image-generators texture-profile))))

  (output gpu-texture g/Any :cached
          (g/fnk [gpu-texture-generator]
            (texture-util/generate-gpu-texture gpu-texture-generator)))

  (output build-targets g/Any :cached produce-build-targets)
  (output transform-properties g/Any scene/produce-no-transform-properties)
  (output save-value g/Any :cached produce-save-value)
  (output scene g/Any :cached produce-scene))

(defn load-cubemap [project self resource cubemap]
  {:pre [(map? cubemap)]} ; Graphics$Cubemap in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      (g/connect project :build-settings self :build-settings)
      (g/connect project :texture-profiles self :texture-profiles)
      (gu/set-properties-from-pb-map self Graphics$Cubemap cubemap
        right (resolve-resource :right)
        left (resolve-resource :left)
        top (resolve-resource :top)
        bottom (resolve-resource :bottom)
        front (resolve-resource :front)
        back (resolve-resource :back)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "cubemap"
    :label (localization/message "resource.type.cubemap")
    :build-ext "texturec"
    :node-type CubemapNode
    :ddf-type Graphics$Cubemap
    :load-fn load-cubemap
    :icon cubemap-icon
    :icon-class :design
    :view-types [:scene :text]))
