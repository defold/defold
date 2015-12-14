(ns editor.cubemap
  (:require [dynamo.file.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]))

(def cubemap-icon "icons/32/Icons_23-Cubemap.png")

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

(def unit-sphere
  (let [lats 16
        longs 32
        vbuf (->normal-vtx (* 6 (* lats longs)))]
    (doseq [face (geom/unit-sphere-pos-nrm lats longs)
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
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(g/defnk produce-gpu-texture
  [_node-id right-img left-img top-img bottom-img front-img back-img]
  (apply texture/image-cubemap-texture _node-id [right-img left-img top-img bottom-img front-img back-img]))

(g/defnk produce-save-data [resource right left top bottom front back]
  {:resource resource
   :content (-> (doto (Graphics$Cubemap/newBuilder)
                  (.setRight right)
                  (.setLeft left)
                  (.setTop top)
                  (.setBottom bottom)
                  (.setFront front)
                  (.setBack back))
              (.build)
              (protobuf/pb->str))})

(g/defnk produce-scene
  [_node-id aabb gpu-texture vertex-binding]
  (let [vertex-binding (vtx/use-with _node-id unit-sphere cubemap-shader)]
    {:node-id    _node-id
     :aabb       aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (let [camera (:camera render-args)]
                                 (render-cubemap gl render-args camera gpu-texture vertex-binding)))
                  :passes [pass/transparent]}}))

(g/defnode CubemapNode
  (inherits project/ResourceNode)
  (inherits scene/SceneNode)

  (property right  g/Str)
  (property left   g/Str)
  (property top    g/Str)
  (property bottom g/Str)
  (property front  g/Str)
  (property back   g/Str)

  (input right-img  BufferedImage)
  (input left-img   BufferedImage)
  (input top-img    BufferedImage)
  (input bottom-img BufferedImage)
  (input front-img  BufferedImage)
  (input back-img   BufferedImage)

  (output gpu-texture g/Any :cached produce-gpu-texture)
  (output save-data   g/Any :cached produce-save-data)
  (output aabb        AABB  :cached (g/always geom/unit-bounding-box))
  (output scene       g/Any :cached produce-scene))

(defn load-cubemap [project self resource]
  (let [cubemap-message (protobuf/pb->map (protobuf/read-text Graphics$Cubemap resource))]
    (for [[side input] cubemap-message
          :let [img-resource (workspace/resolve-resource resource input)]]
      (concat
        (project/connect-resource-node project
                                       img-resource self
                                       [[:content (keyword (subs (str side "-img") 1))]])
        (g/set-property self side input)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "cubemap"
                                    :label "Cubemap"
                                    :node-type CubemapNode
                                    :load-fn load-cubemap
                                    :icon cubemap-icon
                                    :view-types [:scene]))
