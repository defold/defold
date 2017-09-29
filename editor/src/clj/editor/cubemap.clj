(ns editor.cubemap
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap]
           [com.jogamp.opengl GL GL2]
           [editor.types AABB]
           [java.awt.image BufferedImage]))

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
  (gl/with-gl-bindings gl render-args [gpu-texture cubemap-shader vertex-binding]
    (shader/set-uniform cubemap-shader gl "world" geom/Identity4d)
    (shader/set-uniform cubemap-shader gl "cameraPosition" (types/position camera))
    (shader/set-uniform cubemap-shader gl "envMap" 0)
    (gl/gl-enable gl GL/GL_CULL_FACE)
    (gl/gl-cull-face gl GL/GL_BACK)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* sphere-lats sphere-longs)))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(g/defnk produce-gpu-texture
  [_node-id right-image left-image top-image bottom-image front-image back-image]
  (apply texture/image-cubemap-texture _node-id [right-image left-image top-image bottom-image front-image back-image]))

(g/defnk produce-save-value [right left top bottom front back]
  {:right (resource/resource->proj-path right)
   :left (resource/resource->proj-path left)
   :top (resource/resource->proj-path top)
   :bottom (resource/resource->proj-path bottom)
   :front (resource/resource->proj-path front)
   :back (resource/resource->proj-path back)})

(g/defnk produce-scene
  [_node-id aabb gpu-texture]
  (let [vertex-binding (vtx/use-with _node-id unit-sphere cubemap-shader)]
    {:node-id    _node-id
     :aabb       aabb
     :renderable {:render-fn (fn [gl render-args _renderables _count]
                               (let [camera (:camera render-args)]
                                 (render-cubemap gl render-args camera gpu-texture vertex-binding)))
                  :passes [pass/transparent]}}))

(defn- cubemap-side-setter [resource-label image-label]
  (fn [evaluation-context self old-value new-value]
    (project/resource-setter self old-value new-value
                             [:resource resource-label]
                             [:content image-label])))

(g/defnode CubemapNode
  (inherits resource-node/ResourceNode)
  (inherits scene/SceneNode)

  (property right resource/Resource
            (value (gu/passthrough right-resource))
            (set (cubemap-side-setter :right-resource :right-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? right)))
  (property left resource/Resource
            (value (gu/passthrough left-resource))
            (set (cubemap-side-setter :left-resource :left-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? left)))
  (property top resource/Resource
            (value (gu/passthrough top-resource))
            (set (cubemap-side-setter :top-resource :top-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? top)))
  (property bottom resource/Resource
            (value (gu/passthrough bottom-resource))
            (set (cubemap-side-setter :bottom-resource :bottom-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? bottom)))
  (property front resource/Resource
            (value (gu/passthrough front-resource))
            (set (cubemap-side-setter :front-resource :front-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? front)))
  (property back resource/Resource
            (value (gu/passthrough back-resource))
            (set (cubemap-side-setter :back-resource :back-image))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-resource-missing? back)))

  (input right-resource  resource/Resource)
  (input left-resource   resource/Resource)
  (input top-resource    resource/Resource)
  (input bottom-resource resource/Resource)
  (input front-resource  resource/Resource)
  (input back-resource   resource/Resource)

  (input right-image  BufferedImage)
  (input left-image   BufferedImage)
  (input top-image    BufferedImage)
  (input bottom-image BufferedImage)
  (input front-image  BufferedImage)
  (input back-image   BufferedImage)

  (output transform-properties g/Any scene/produce-no-transform-properties)
  (output gpu-texture g/Any :cached produce-gpu-texture)
  (output save-value  g/Any :cached produce-save-value)
  (output aabb        AABB  :cached (g/constantly geom/unit-bounding-box))
  (output scene       g/Any :cached produce-scene))

(defn load-cubemap [_project self resource cubemap-message]
  (for [[side input] cubemap-message
        :let [image-resource (workspace/resolve-resource resource input)]]
    (g/set-property self side image-resource)))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "cubemap"
    :label "Cubemap"
    :node-type CubemapNode
    :ddf-type Graphics$Cubemap
    :load-fn load-cubemap
    :icon cubemap-icon
    :view-types [:scene :text]))
