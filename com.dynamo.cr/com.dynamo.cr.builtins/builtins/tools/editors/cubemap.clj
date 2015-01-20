(ns editors.cubemap
  (:require [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.background :as background]
            [dynamo.camera :as c]
            [dynamo.editors :as ed]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as g]
            [dynamo.gl :as gl :refer [do-gl]]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.image :as img :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.texture :refer :all]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.node :as in]
            [internal.ui.grid :as grid]
            [internal.render.pass :as pass])
  (:import  [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2]
            [javax.vecmath Matrix4d Matrix4f Vector4f]
            [org.eclipse.core.commands ExecutionEvent]
            [dynamo.types AABB Camera]))

(n/defnode CubemapImageInputs
  (input image-right  Image)
  (input image-left   Image)
  (input image-top    Image)
  (input image-bottom Image)
  (input image-front  Image)
  (input image-back   Image))

(n/defnode CubemapImageProperties
  (property right  s/Str)
  (property left   s/Str)
  (property top    s/Str)
  (property bottom s/Str)
  (property front  s/Str)
  (property back   s/Str))

(defnk produce-image-right  [image-right]  image-right)
(defnk produce-image-left   [image-left]   image-left)
(defnk produce-image-top    [image-top]    image-top)
(defnk produce-image-bottom [image-bottom] image-bottom)
(defnk produce-image-front  [image-front]  image-front)
(defnk produce-image-back   [image-back]   image-back)

(n/defnode CubemapImageOutputs
  (output image-right  Image produce-image-right)
  (output image-left   Image produce-image-left)
  (output image-top    Image produce-image-top)
  (output image-bottom Image produce-image-bottom)
  (output image-front  Image produce-image-front)
  (output image-back   Image produce-image-back))

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

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

(defnk produce-shader :- s/Int
  [this gl-context gl]
  (shader/make-shader gl-context gl pos-norm-vert pos-norm-frag))

(defnk produce-gpu-texture
  [project this gl image-right image-left image-top image-bottom image-front image-back]
  (let [texture (texture/image-cubemap-texture gl (:contents image-right) (:contents image-left) (:contents image-top) (:contents image-bottom) (:contents image-front) (:contents image-back))]
    texture))

(defnk produce-renderable-vertex-buffer
  []
  (let [lats 16
        longs 32
        vbuf (->normal-vtx (* 6 (* lats longs)))]
    (doseq [face (g/unit-sphere-pos-nrm lats longs)
            v    face]
      (conj! vbuf v))
    (persistent! vbuf)))

(defn render-cubemap
  [ctx ^GL2 gl this project world]
  (do-gl [this            (assoc this :gl-context ctx)
          this            (assoc this :gl gl)
          texture         (n/get-node-value this :gpu-texture)
          shader          (n/get-node-value this :shader)
          vbuf            (n/get-node-value this :vertex-buffer)
          vertex-binding  (vtx/use-with gl vbuf shader)
          camera          (in/get-inputs this (-> this :world-ref deref :graph) :camera)]
         (shader/set-uniform shader "world" world)
         (shader/set-uniform shader "cameraPosition" (t/position camera))
         (shader/set-uniform shader "envMap" (texture/texture-unit-index texture))
         (gl/gl-enable gl GL/GL_CULL_FACE)
         (gl/gl-cull-face gl GL/GL_BACK)
         (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
         (gl/gl-disable gl GL/GL_CULL_FACE)))

(defnk produce-renderable :- RenderData
  [this project vertex-buffer]
  (let [world (Matrix4d. g/Identity4d)]
    {pass/transparent
     [{:world-transform world
       :render-fn       (fn [ctx gl glu text-renderer] (render-cubemap ctx gl this project world))}]}))

(defnk unit-bounding-box
  []
  (-> (g/null-aabb)
    (g/aabb-incorporate  1  1  1)
    (g/aabb-incorporate -1 -1 -1)))

(n/defnode CubemapRender
  (inherits CubemapImageInputs)
  (input  camera        Camera)
  (output shader        s/Any      :cached produce-shader)
  (output vertex-buffer s/Any      :cached produce-renderable-vertex-buffer)
  (output gpu-texture   s/Any      :cached produce-gpu-texture)
  (output renderable    RenderData :cached produce-renderable)
  (output aabb          AABB               unit-bounding-box))

(def ^:private cubemap-inputs
  {:right  :image-right
   :left   :image-left
   :top    :image-top
   :bottom :image-bottom
   :front  :image-front
   :back   :image-back})

(n/defnode CubemapNode
  (inherits n/OutlineNode)
  (inherits CubemapImageProperties)
  (inherits CubemapImageInputs)
  (inherits CubemapImageOutputs)

  (on :load
    (let [project         (:project event)
          cubemap-message (protobuf/pb->map (protobuf/read-text Graphics$Cubemap (:filename self)))]
      (doseq [[side input] cubemap-message]
        (let [img-node (t/lookup project input)]
          (ds/set-property self side input)
          (ds/connect img-node :image self (cubemap-inputs side))
          (ds/connect img-node :tree  self :children))))))

(defn- connect-cubemap-inputs [source-node dest-node]
  (doseq [label (vals cubemap-inputs)]
    (ds/connect source-node label dest-node label)))

(defn on-edit
  [project-node editor-site cubemap]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
      (let [cubemap-render (ds/add (n/construct CubemapRender))
            background     (ds/add (n/construct background/Gradient))
            grid           (ds/add (n/construct grid/Grid))
            camera         (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
        (connect-cubemap-inputs cubemap cubemap-render)
        (ds/connect camera         :camera     grid           :camera)
        (ds/connect camera         :camera     editor         :view-camera)
        (ds/connect camera         :self       editor         :controller)
        (ds/connect background     :renderable editor         :renderables)
        (ds/connect cubemap-render :renderable editor         :renderables)
        (ds/connect camera         :camera     cubemap-render :camera)
        (ds/connect grid           :renderable editor         :renderables)
        (ds/connect cubemap-render :aabb       editor         :aabb))
      editor)))

(when (ds/in-transaction?)
  (p/register-editor "cubemap" #'on-edit)
  (p/register-node-type "cubemap" CubemapNode))
