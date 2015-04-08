(ns editor.sprite
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [internal.render.pass :as pass])
  (:import [com.dynamo.sprite.proto Sprite Sprite$SpriteDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]
           [java.io PushbackReader]))

(def sprite-icon "icons/pictures.png")

; Render assets

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0)
  (vec4 color))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor (* var_color (texture2D texture var_texcoord0.xy)))
    #_(setq gl_FragColor (vec4 var_texcoord0.xy 0 1))
    ))

(def shader (shader/make-shader vertex-shader fragment-shader))

; Rendering

#_(defn render-text
   [ctx ^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc ^Float zloc ^Float scale]
   (gl/gl-push-matrix gl
     (.glScaled gl 1 -1 1)
     (.setColor text-renderer 1 1 1 1)
     (.begin3DRendering text-renderer)
     (.draw3D text-renderer chars xloc yloc zloc scale)
     (.end3DRendering text-renderer)))

#_(defn render-palette [ctx ^GL2 gl text-renderer gpu-texture vertex-binding layout]
   (doseq [group layout]
     (render-text ctx gl text-renderer (:name group) (- (:x group) palette-cell-size-half) (+ (* 0.25 group-spacing) (- palette-cell-size-half (:y group))) 0 1))
   (let [cell-count (reduce (fn [v0 v1] (+ v0 (count (:cells v1)))) 0 layout)]
    (gl/with-enabled gl [gpu-texture shader vertex-binding]
     (shader/set-uniform shader gl "texture" 0)
     (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 cell-count)))))

(defn render-sprite [^GL2 gl gpu-texture vertex-binding]
 (gl/with-enabled gl [gpu-texture shader vertex-binding]
   (shader/set-uniform shader gl "texture" 0)
   (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)))

; Vertex generation

(defn anim-uvs [textureset anim]
  (let [frame (first (:frames anim))]
    (if-let [{start :tex-coords-start count :tex-coords-count} frame]
      (mapv #(nth (:tex-coords textureset) %) (range start (+ start count)))
      [[0 0] [0 0]])))

(defn gen-vertex [x y u v color]
  (doall (concat [x y 0 1 u v] color)))

(defn gen-quad [textureset animation]
  (let [x1 (* 0.5 (:width animation))
        y1 (* 0.5 (:height animation))
        x0 (- x1)
        y0 (- y1)
        [[u0 v0] [u1 v1]] (anim-uvs textureset animation)
        color [1 1 1 1]]
    [(gen-vertex x0 y0 u0 v1 color)
     (gen-vertex x1 y0 u1 v1 color)
     (gen-vertex x0 y1 u0 v0 color)
     (gen-vertex x1 y0 u1 v1 color)
     (gen-vertex x1 y1 u1 v0 color)
     (gen-vertex x0 y1 u0 v0 color)]))

(defn gen-vertex-buffer
  [textureset animation]
  (let [vbuf  (->texture-vtx 6)]
    (doseq [vertex (gen-quad textureset animation)]
      (conj! vbuf vertex))
    (persistent! vbuf)))

; Node defs

(g/defnk produce-renderable
  [this gpu-texture vertex-binding]
  {pass/transparent [{:world-transform geom/Identity4d
                   :render-fn (fn [ctx gl glu text-renderer]
                                (render-sprite gl gpu-texture vertex-binding))}]})

(g/defnode SpriteRender
  (input textureset t/Any)
  (input gpu-texture t/Any)
  (input animation t/Any)

  (output vertex-binding t/Any :cached (g/fnk [textureset animation] (vtx/use-with (gen-vertex-buffer textureset animation) shader)))
  (output renderable             t/RenderData  produce-renderable))

(g/defnode SpriteNode
  (inherits project/ResourceNode)

  (property image t/Str)
  (property default-animation t/Str)
  (property material t/Str)
  (property blend-mode t/Any (default :BLEND_MODE_ALPHA))

  (input textureset t/Any)
  (input gpu-texture t/Any)

  (output textureset t/Any (g/fnk [textureset] textureset))
  (output gpu-texture t/Any (g/fnk [gpu-texture] gpu-texture))
  (output animation t/Any (g/fnk [textureset default-animation] (get (:animations textureset) default-animation))) ; TODO - use placeholder animation
  (output aabb AABB (g/fnk [animation] (let [hw (* 0.5 (:width animation))
                                            hh (* 0.5 (:height animation))]
                                        (-> (geom/null-aabb)
                                          (geom/aabb-incorporate (Point3d. (- hw) (- hh) 0))
                                          (geom/aabb-incorporate (Point3d. hw hh 0))))))
  (output outline t/Any (g/fnk [self] {:self self :label "Sprite" :icon sprite-icon})))

(defn load-sprite [project self input]
  (let [sprite (protobuf/pb->map (protobuf/read-text Sprite$SpriteDesc input))]
    (concat
      (g/set-property self :image (:tile-set sprite))
      (g/set-property self :default-animation (:default-animation sprite))
      (g/set-property self :material (:material sprite))
      ; TODO - hack, protobuf should carry the default
      (if (:blend-mode sprite)
        (g/set-property self :blend-mode (:blend-mode sprite))
        [])
      (let [atlas-node (project/resolve-resource-node self (:tile-set sprite))]
        (if atlas-node
          [(g/connect atlas-node :textureset self :textureset)
           (g/connect atlas-node :gpu-texture self :gpu-texture)]
          [])))))

(defn setup-rendering [self view]
  (let [renderer   (t/lookup view :renderer)
        camera     (t/lookup view :camera)
        view-graph (g/nref->gid (g/node-id view))]
    (g/make-nodes view-graph [sprite-render SpriteRender]
                  (g/connect sprite-render   :renderable    renderer        :renderables)
                  (g/connect self            :textureset    sprite-render   :textureset)
                  (g/connect self            :animation     sprite-render   :animation)
                  (g/connect self            :gpu-texture   sprite-render   :gpu-texture)
                  (g/connect self            :aabb          camera          :aabb))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "sprite"
                                    :node-type SpriteNode
                                    :load-fn load-sprite
                                    :icon sprite-icon
                                    :view-types [:scene]
                                    :view-fns {:scene {:setup-view-fn (fn [self view] (scene/setup-view view))
                                                       :setup-rendering-fn setup-rendering}}))
