(ns editors.cubemap
  (:require [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [dynamo.background :as background]
            [dynamo.camera :as c]
            [dynamo.editors :as ed]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.grid :as grid]
            [dynamo.image :as img]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2]
            [javax.vecmath Matrix4d]
            [dynamo.types AABB Camera Image]))

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

(def unit-sphere
  (let [lats 16
        longs 32
        vbuf (->normal-vtx (* 6 (* lats longs)))]
    (doseq [face (g/unit-sphere-pos-nrm lats longs)
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

(def cubemap-shader (shader/make-shader pos-norm-vert pos-norm-frag))

(defn render-cubemap
  [^GL2 gl world camera gpu-texture vertex-binding]
  (gl/with-enabled gl [gpu-texture cubemap-shader vertex-binding]
    (shader/set-uniform cubemap-shader gl "world" world)
    (shader/set-uniform cubemap-shader gl "cameraPosition" (t/position camera))
    (shader/set-uniform cubemap-shader gl "envMap" (texture/texture-unit-index gl gpu-texture))
    (gl/gl-enable gl GL/GL_CULL_FACE)
    (gl/gl-cull-face gl GL/GL_BACK)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(defnk produce-renderable :- RenderData
  [this camera gpu-texture vertex-binding]
  (let [world (Matrix4d. g/Identity4d)]
    {pass/transparent
     [{:world-transform world
       :render-fn       (fn [ctx gl glu text-renderer] (render-cubemap gl world camera gpu-texture vertex-binding))}]}))

(n/defnode CubemapRender
  (input gpu-texture s/Any)

  (input  camera        Camera)

  (output vertex-binding s/Any     :cached (fnk [] (vtx/use-with unit-sphere cubemap-shader)))
  (output renderable    RenderData :cached produce-renderable)
  (output aabb          AABB       :cached (fnk [] g/unit-bounding-box)))

(defnk produce-gpu-texture
  [right left top bottom front back]
  (apply texture/image-cubemap-texture (map :contents [right left top bottom front back])))

(n/defnode CubemapNode
  (inherits n/AutowireResources)
  (inherits n/OutlineNode)

  (property right  dp/ImageResource)
  (property left   dp/ImageResource)
  (property top    dp/ImageResource)
  (property bottom dp/ImageResource)
  (property front  dp/ImageResource)
  (property back   dp/ImageResource)

  (output gpu-texture   s/Any  :cached produce-gpu-texture)

  (on :load
    (let [cubemap-message (protobuf/pb->map (protobuf/read-text Graphics$Cubemap (:filename self)))]
      (doseq [[side input] cubemap-message]
        (ds/set-property self side input)))))

(defn on-edit
  [project-node editor-site cubemap]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
      (let [cubemap-render (ds/add (n/construct CubemapRender))
            background     (ds/add (n/construct background/Gradient))
            grid           (ds/add (n/construct grid/Grid))
            camera         (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
        (ds/connect cubemap        :gpu-texture cubemap-render :gpu-texture)
        (ds/connect camera         :camera      grid           :camera)
        (ds/connect camera         :camera      editor         :view-camera)
        (ds/connect camera         :self        editor         :controller)
        (ds/connect background     :renderable  editor         :renderables)
        (ds/connect cubemap-render :renderable  editor         :renderables)
        (ds/connect camera         :camera      cubemap-render :camera)
        (ds/connect grid           :renderable  editor         :renderables)
        (ds/connect cubemap-render :aabb        editor         :aabb))
      editor)))

(when (ds/in-transaction?)
  (p/register-editor "cubemap" #'on-edit)
  (p/register-node-type "cubemap" CubemapNode))
