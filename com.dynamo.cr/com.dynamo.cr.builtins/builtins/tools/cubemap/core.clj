(ns cubemap.core
  (:require [dynamo.ui :refer :all]
            [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.buffers :refer :all]
            [dynamo.camera :as c]
            [dynamo.env :as e]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters pb->str]]
            [dynamo.geom :as g :refer [to-short-uv unit-sphere-pos-nrm]]
            [dynamo.gl :as gl :refer [do-gl]]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.protocols :as glp]
            [dynamo.gl.vertex :as vtx]
            [dynamo.image :as img :refer :all]
            [dynamo.node :as n]
            [dynamo.outline :refer :all]
            [dynamo.project :as p]
            [dynamo.system :as ds :refer [transactional in current-scope add in-transaction? connect]]
            [dynamo.texture :refer :all]
            [dynamo.types :refer :all]
            [dynamo.ui :refer [defcommand defhandler]]
            [internal.ui.background :as background]
            [internal.ui.grid :as grid]
            [internal.ui.scene-editor :as ise]
            [internal.ui.menus :as menus]
            [internal.ui.handlers :as handlers]
            [internal.render.pass :as pass]
            [service.log :as log :refer [logging-exceptions]]
            [camel-snake-kebab :refer :all]
            [clojure.osgi.core :refer [*bundle*]])
  (:import  [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2]
            [javax.vecmath Matrix4d Matrix4f Vector4f]
            [org.eclipse.core.commands ExecutionEvent]))

(n/defnode CubemapProperties
  (input image-right  Image)
  (input image-left   Image)
  (input image-top    Image)
  (input image-bottom Image)
  (input image-front  Image)
  (input image-back   Image)

  (property right  String)
  (property left   String)
  (property top    String)
  (property bottom String)
  (property front  String)
  (property back   String))

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
  (uniform vec4 cameraPosition)
  (uniform samplerCube envMap)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vec3 camToV (normalize (- vWorld (.xyz cameraPosition))))
    (setq vec3 refl (reflect camToV vNormal))
      (setq gl_FragColor (textureCube envMap refl))))

(defnk produce-shader :- s/Int
  [this gl]
  (shader/make-shader gl pos-norm-vert pos-norm-frag))

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
  (do-gl [this            (assoc this :gl gl)
          texture         (n/get-node-value this :gpu-texture)
          shader          (n/get-node-value this :shader)
          vbuf            (n/get-node-value this :vertex-buffer)
          vertex-binding  (vtx/use-with gl vbuf shader)]
         (shader/set-uniform shader "world" world)
         (shader/set-uniform shader "cameraPosition" (doto (Vector4f.) (.set 0.0 0.0 4 1.0)))
         (shader/set-uniform shader "envMap" (texture/texture-unit-index texture))
         (gl/gl-enable gl GL/GL_CULL_FACE)
         (gl/gl-cull-face gl GL/GL_BACK)
         (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
         (gl/gl-disable gl GL/GL_CULL_FACE)))

(defnk produce-renderable :- RenderData
  [this project]
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
  (output shader        s/Any      :cached produce-shader)
  (output vertex-buffer s/Any      :cached produce-renderable-vertex-buffer)
  (output gpu-texture   s/Any      :cached produce-gpu-texture)
  (output renderable    RenderData :cached produce-renderable)
  (output aabb          t/AABB             unit-bounding-box))

(n/defnode CubemapNode
  (inherits CubemapProperties)
  (inherits CubemapRender))

(protocol-buffer-converters
 Graphics$Cubemap
 {:constructor #'cubemap.core/make-cubemap-node
  :basic-properties [:right :left :top :bottom :front :back]})

(defn- make-face
  [cubemap side input]
  (ds/connect (ds/add (img/make-image-source :image (get cubemap side))) :image cubemap input))

(def ^:private cubemap-inputs
  {:right  :image-right
   :left   :image-left
   :top    :image-top
   :bottom :image-bottom
   :front  :image-front
   :back   :image-back})

(defn on-load
  [path ^Graphics$Cubemap cubemap-message]
  (let [cubemap (message->node cubemap-message :filename path :_id -1)]
    (doseq [side [:right :left :top :bottom :front :back]]
      (make-face cubemap side (get cubemap-inputs side)))))

(defn on-edit
  [world-ref project-node editor-site file]
  (let [cubemap (p/node-by-filename project-node file)
        editor  (ise/make-scene-editor :name "editor")]
    (ds/transactional
      (ds/in (ds/add editor)
        (let [background (ds/add (background/make-background))
              grid       (ds/add (grid/make-grid))
              camera     (ds/add (c/make-camera-node :camera (c/make-camera :orthographic)))
              controller (ds/add (c/make-camera-controller))]
          (ds/connect camera     :camera     grid       :camera)
          (ds/connect camera     :camera     editor     :view-camera)
          (ds/connect controller :self       editor     :controller)
          (ds/connect camera     :camera     controller :camera)
          (ds/connect background :renderable editor     :renderables)
          (ds/connect cubemap    :renderable editor     :renderables)
          (ds/connect grid       :renderable editor     :renderables)
          (ds/connect cubemap    :aabb       editor     :aabb))
        editor))))

(p/register-editor "cubemap" #'on-edit)
(p/register-loader "cubemap" (protocol-buffer-loader Graphics$Cubemap on-load))
