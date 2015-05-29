(ns editor.sprite
  (:require [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sprite.proto Sprite Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def sprite-icon "icons/pictures.png")

; Render assets

(vtx/defvertex texture-vtx
  (vec3 position)
  (vec2 texcoord0 true))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform vec4 color)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (* color (texture2D texture var_texcoord0.xy)))
    #_(setq gl_FragColor (vec4 var_texcoord0.xy 0 1))
    ))

(def shader (shader/make-shader vertex-shader fragment-shader))

(shader/defshader outline-vertex-shader
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader outline-fragment-shader
  (uniform vec4 color)
  (defn void main []
    (setq gl_FragColor color)))

(def outline-shader (shader/make-shader outline-vertex-shader outline-fragment-shader))

; Rendering

(defn render-sprite [^GL2 gl gpu-texture vertex-binding outline-vertex-binding pass selected blend-mode]
  (let [color (float-array (scene/select-color pass selected [1.0 1.0 1.0]))]
    (if (= pass pass/outline)
      (gl/with-enabled gl [outline-shader outline-vertex-binding]
        (shader/set-uniform outline-shader gl "color" color)
        (gl/gl-draw-arrays gl GL/GL_LINE_LOOP 0 4))
      (gl/with-enabled gl [gpu-texture shader vertex-binding]
        (when (= pass pass/transparent)
          (case blend-mode
            :BLEND_MODE_ALPHA (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (:BLEND_MODE_ADD :BLEND_MODE_ADD_ALPHA) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
            :BLEND_MODE_MULT (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR)))
        (shader/set-uniform shader gl "color" color)
        (shader/set-uniform shader gl "texture" 0)
        (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)
        (when (= pass pass/transparent)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))))

; Vertex generation

(defn anim-uvs [textureset anim]
  (let [frame (first (:frames anim))]
    (if-let [{start :tex-coords-start count :tex-coords-count} frame]
      (mapv #(nth (:tex-coords textureset) %) (range start (+ start count)))
      [[0 0] [0 0]])))

(defn gen-vertex [x y u v]
  [x y 0 u v])

(defn gen-quad [textureset animation]
  (let [x1 (* 0.5 (:width animation))
        y1 (* 0.5 (:height animation))
        x0 (- x1)
        y0 (- y1)
        [[u0 v0] [u1 v1]] (anim-uvs textureset animation)]
    [(gen-vertex x0 y0 u0 v1)
     (gen-vertex x1 y0 u1 v1)
     (gen-vertex x0 y1 u0 v0)
     (gen-vertex x1 y0 u1 v1)
     (gen-vertex x1 y1 u1 v0)
     (gen-vertex x0 y1 u0 v0)]))

(defn gen-outline-quad [textureset animation]
  (let [x1 (* 0.5 (:width animation))
        y1 (* 0.5 (:height animation))
        x0 (- x1)
        y0 (- y1)
        [[u0 v0] [u1 v1]] (anim-uvs textureset animation)]
    [(gen-vertex x0 y0 u0 v1)
     (gen-vertex x1 y0 u1 v1)
     (gen-vertex x1 y1 u1 v0)
     (gen-vertex x0 y1 u0 v0)]))

(defn gen-vertex-buffer
  [textureset animation gen-quad-fn vtx-count]
  (let [vbuf  (->texture-vtx vtx-count)]
    (doseq [vertex (gen-quad-fn textureset animation)]
      (conj! vbuf vertex))
    (persistent! vbuf)))

; Node defs

(g/defnk produce-save-data [self image default-animation material blend-mode]
  {:resource (:resource self)
   :content (protobuf/pb->str
              (.build
                (doto (Sprite$SpriteDesc/newBuilder)
                  (.setTileSet (workspace/proj-path image))
                  (.setDefaultAnimation default-animation)
                  (.setMaterial (workspace/proj-path material))
                  (.setBlendMode (protobuf/val->pb-enum Sprite$SpriteDesc$BlendMode blend-mode)))))})

(g/defnk produce-scene
  [self aabb gpu-texture textureset animation blend-mode]
  (let [scene {:node-id (g/node-id self)
               :aabb aabb}]
    (if animation
      (let [vertex-binding (vtx/use-with (gen-vertex-buffer textureset animation gen-quad 6) shader)
            outline-vertex-binding (vtx/use-with (gen-vertex-buffer textureset animation gen-outline-quad 4) outline-shader)]
        (assoc scene :renderable {:render-fn (g/fnk [gl pass selected] (render-sprite gl gpu-texture vertex-binding outline-vertex-binding pass selected blend-mode))
                                  :passes [pass/transparent pass/selection pass/outline]}))
     scene)))

(defn- connect-atlas [project self image]
  (if-let [atlas-node (project/get-resource-node project image)]
    (do
      [(g/connect atlas-node :textureset self :textureset)
      (g/connect atlas-node :gpu-texture self :gpu-texture)])
    []))

(defn- disconnect-all [self label]
  (let [sources (g/sources-of self label)]
    (for [[src-node src-label] sources]
      (g/disconnect src-node src-label self label))))

(defn reconnect [transaction graph self label kind labels]
  (when (some #{:image} labels)
    (let [image (:image self)
          project (:parent self)]
      (concat
        (disconnect-all self :textureset)
        (disconnect-all self :gpu-texture)
        (connect-atlas project self image)))))

(g/defnode SpriteNode
  (inherits project/ResourceNode)

  (property image (t/protocol workspace/Resource))
  (property default-animation t/Str)
  (property material (t/protocol workspace/Resource))
  (property blend-mode t/Any (default :BLEND_MODE_ALPHA) (tag Sprite$SpriteDesc$BlendMode))

  (trigger reconnect :property-touched #'reconnect)

  (input textureset t/Any)
  (input gpu-texture t/Any)

  (output textureset t/Any (g/fnk [textureset] textureset))
  (output gpu-texture t/Any (g/fnk [gpu-texture] gpu-texture))
  (output animation t/Any (g/fnk [textureset default-animation] (get (:animations textureset) default-animation))) ; TODO - use placeholder animation
  (output aabb AABB (g/fnk [animation] (if animation
                                         (let [hw (* 0.5 (:width animation))
                                               hh (* 0.5 (:height animation))]
                                           (-> (geom/null-aabb)
                                             (geom/aabb-incorporate (Point3d. (- hw) (- hh) 0))
                                             (geom/aabb-incorporate (Point3d. hw hh 0))))
                                         (geom/null-aabb))))
  (output outline t/Any (g/fnk [self] {:node-id (g/node-id self) :label "Sprite" :icon sprite-icon}))
  (output save-data t/Any :cached produce-save-data)
  (output scene t/Any :cached produce-scene))

(defn load-sprite [project self input]
  (let [sprite (protobuf/pb->map (protobuf/read-text Sprite$SpriteDesc input))
        resource (:resource self)
        image (workspace/resolve-resource resource (:tile-set sprite))
        material (workspace/resolve-resource resource (:material sprite))]
    (concat
      (g/set-property self :image image)
      (g/set-property self :default-animation (:default-animation sprite))
      (g/set-property self :material material)
      (g/set-property self :blend-mode (:blend-mode sprite))
      (connect-atlas project self image))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "sprite"
                                    :node-type SpriteNode
                                    :load-fn load-sprite
                                    :icon sprite-icon
                                    :view-types [:scene]
                                    :tags #{:component}
                                    :template "templates/template.sprite"))
