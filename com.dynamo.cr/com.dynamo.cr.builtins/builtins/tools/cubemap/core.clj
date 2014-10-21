(ns cubemap.core
  (:require [dynamo.ui :refer :all]
            [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.buffers :refer :all]
            [dynamo.env :as e]
            [dynamo.geom :as g :refer [to-short-uv unit-sphere-pos-nrm]]
            [dynamo.gl :as gl :refer [do-gl]]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.protocols :as glp]
            [dynamo.gl.vertex :as vtx]
            [dynamo.node :refer :all]
            [dynamo.scene-editor :refer [dynamic-scene-editor]]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters pb->str]]
            [dynamo.project :as p :refer [register-loader register-editor transact new-resource connect query node-by-id resolve-tempid]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [dynamo.outline :refer :all]
            [dynamo.ui :refer [defcommand defhandler]]
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

(defnode CubemapProperties
  (input image-right Image)
  (input image-left Image)
  (input image-top Image)
  (input image-bottom Image)
  (input image-front Image)
  (input image-back Image)

  (property right  (string))
  (property left   (string))
  (property top    (string))
  (property bottom (string))
  (property front  (string))
  (property back   (string)))

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
          texture         (p/get-node-value this :gpu-texture)
          shader          (p/get-node-value this :shader)
          vbuf            (p/get-node-value this :vertex-buffer)
          vertex-binding  (vtx/use-with gl vbuf shader)]
         (shader/set-uniform shader "world" world)
         (shader/set-uniform shader "cameraPosition" (doto (Vector4f.) (.set 0.0 0.0 4 1.0)))
         (shader/set-uniform shader "envMap" (texture/texture-unit-index texture))
         (gl/gl-enable gl GL/GL_CULL_FACE)
         (gl/gl-cull-face gl GL/GL_BACK)
         (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
         (gl/gl-disable gl GL/GL_CULL_FACE)))

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer this project image]
   (let [img ^BufferedImage (:contents image)]
     (gl/overlay ctx gl text-renderer (format "Size: %dx%d" (.getWidth img) (.getHeight img)) 12.0 -22.0 1.0 1.0)))

(defnk produce-renderable :- RenderData
  [this project image-right]
  (let [world (Matrix4d. g/Identity4d)]
    {pass/overlay
    [{:world-transform g/Identity4d  :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer this project image-right))}]
    pass/transparent
    [{:world-transform world  :render-fn       (fn [ctx gl glu text-renderer] (render-cubemap ctx gl this project world))}]}))

(defnode CubemapRender
  (output shader s/Any                :cached produce-shader)
  (output vertex-buffer s/Any         :cached produce-renderable-vertex-buffer)
  (output gpu-texture s/Any           :cached produce-gpu-texture)
  (output renderable RenderData       :cached produce-renderable))

(defnode CubemapNode
  (inherits CubemapProperties)
  (inherits CubemapRender))

(protocol-buffer-converters
 Graphics$Cubemap
 {:constructor #'cubemap.core/make-cubemap-node
  :basic-properties [:right :left :top :bottom :front :back]})

(defn on-load
  [project path ^Graphics$Cubemap cubemap-message]
  (let [cubemap-tx (message->node cubemap-message (constantly []) :filename path :_id -1)
        tx-result (transact project cubemap-tx)
        ^CubemapProperties cubemap (node-by-id project (resolve-tempid tx-result -1))
        right (make-image-source :image (:right cubemap))
        left (make-image-source :image (:left cubemap))
        top (make-image-source :image (:top cubemap))
        bottom (make-image-source :image (:bottom cubemap))
        front (make-image-source :image (:front cubemap))
        back (make-image-source :image (:back cubemap))]
    
    (transact project
      [(new-resource right)
       (new-resource left)
       (new-resource top)
       (new-resource bottom)
       (new-resource front)
       (new-resource back)
       (connect right :image cubemap :image-right)
       (connect left :image cubemap :image-left)
       (connect top :image cubemap :image-top)
       (connect bottom :image cubemap :image-bottom)
       (connect front :image cubemap :image-front)
       (connect back :image cubemap :image-back)])))

(logging-exceptions "Cubemap tooling"
  (register-editor (e/current-project) "cubemap" #'dynamic-scene-editor)
  (register-loader (e/current-project) "cubemap" (protocol-buffer-loader Graphics$Cubemap on-load)))
