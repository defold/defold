(ns editor.atlas
  (:require [clojure.string :as str]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.texture :as tex]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [internal.render.pass :as pass])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
           [com.dynamo.tile.proto Tile$Playback]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]
           [java.nio ByteBuffer ByteOrder]))

(def atlas-icon "icons/images.png")
(def animation-icon "icons/film.png")
(def image-icon "icons/image.png")

(defn render-overlay
  [^GL2 gl ^TextRenderer text-renderer width height]
  (gl/overlay gl text-renderer (format "Size: %dx%d" width height) 12.0 -22.0 1.0 1.0))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(shader/defshader pos-uv-vert
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader pos-uv-frag
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))))

(def atlas-shader (shader/make-shader pos-uv-vert pos-uv-frag))

(defn render-texture-set
  [gl vertex-binding gpu-texture]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)))

(defn- path->id [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn image->animation [image]
  (types/map->Animation {:id              (path->id (:path image))
                   :images          [image]
                   :fps             30
                   :flip-horizontal 0
                   :flip-vertical   0
                   :playback        :PLAYBACK_ONCE_FORWARD}))

(g/defnode AtlasImage
  (property path g/Str)
  (input src-image BufferedImage)
  (output image Image (g/fnk [path ^BufferedImage src-image] (Image. path src-image (.getWidth src-image) (.getHeight src-image))))
  (output animation Animation (g/fnk [image] (image->animation image)))
  (output outline g/Any (g/fnk [node-id path] {:node-id node-id :label (path->id path) :icon image-icon}))
  (output ddf-message g/Any :cached (g/fnk [path] (.build (doto (AtlasProto$AtlasImage/newBuilder) (.setImage path))))))

(g/defnk produce-anim-ddf [id fps flip-horizontal flip-vertical playback img-ddf]
  (.build (doto (AtlasProto$AtlasAnimation/newBuilder)
            (.setId id)
            (.setFps fps)
            (.setFlipHorizontal flip-horizontal)
            (.setFlipVertical flip-vertical)
            (.setPlayback (protobuf/val->pb-enum Tile$Playback playback))
            (.addAllImages img-ddf))))

(g/defnode AtlasAnimation
  (property id g/Str)
  (property fps             types/NonNegativeInt (default 30))
  (property flip-horizontal g/Int)
  (property flip-vertical   g/Int)
  (property playback        types/AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (input frames Image :array)
  (input outline g/Any :array)
  (input img-ddf g/Any :array)

  (output animation Animation (g/fnk [this id frames fps flip-horizontal flip-vertical playback]
                                     (types/->Animation id frames fps flip-horizontal flip-vertical playback)))
  (output outline g/Any (g/fnk [node-id id outline] {:node-id node-id :label id :children outline :icon animation-icon}))
  (output ddf-message g/Any :cached produce-anim-ddf))

(g/defnk produce-save-data [resource margin extrude-borders img-ddf anim-ddf]
  {:resource resource
   :content (-> (doto (AtlasProto$Atlas/newBuilder)
                  (.setMargin margin)
                  (.setExtrudeBorders extrude-borders)
                  (.addAllImages img-ddf)
                  (.addAllAnimations anim-ddf))
              (.build)
              (protobuf/pb->str))})

(defn gen-renderable-vertex-buffer
  [width height]
  (let [x0 0
        y0 0
        x1 width
        y1 height]
    (persistent!
      (doto (->texture-vtx 6)
           (conj! [x0 y0 0 1 0 1])
           (conj! [x0 y1 0 1 0 0])
           (conj! [x1 y1 0 1 1 0])

           (conj! [x1 y1 0 1 1 0])
           (conj! [x1 y0 0 1 1 1])
           (conj! [x0 y0 0 1 0 1])))))

(g/defnk produce-scene
  [self texture-set-data aabb gpu-texture]
  (let [img (:image texture-set-data)
        width (.getWidth img)
        height (.getHeight img)
        vertex-buffer (gen-renderable-vertex-buffer width height)
        vertex-binding (vtx/use-with vertex-buffer atlas-shader)]
    {:aabb aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (let [pass (:pass render-args)
                                     text-renderer (:text-renderer render-args)]
                                 (cond
                                   (= pass pass/overlay)     (render-overlay gl text-renderer width height)
                                   (= pass pass/transparent) (render-texture-set gl vertex-binding gpu-texture))))
                  :passes [pass/overlay pass/transparent]}}))

(g/defnk produce-texture-set-data [animations images margin extrude-borders]
  ; TODO - add inner padding
  (let [inner-padding 0]
    (time (texture-set-gen/->texture-set-data animations images margin inner-padding extrude-borders))))

(defn- ->uv-vertex [vert-index tex-coords]
  (let [index (* vert-index 2)]
    [(.get tex-coords index) (.get tex-coords (inc index))]))

(defn- ->uv-quad [quad-index tex-coords]
  (let [offset (* quad-index 4)]
    (mapv #(->uv-vertex (+ offset %) tex-coords) (range 4))))

(defn- ->anim-frame [frame-index tex-coords]
  {:tex-coords (->uv-quad frame-index tex-coords)})

(defn- ->anim-data [anim tex-coords]
  {:width (:width anim)
   :height (:height anim)
   :frames (mapv #(->anim-frame % tex-coords) (range (:start anim) (:end anim)))})

(g/defnk produce-anim-data [texture-set-data]
  (let [tex-set (:texture-set texture-set-data)
        tex-coords (-> (:tex-coords tex-set)
                     (.asReadOnlyByteBuffer)
                     (.order ByteOrder/LITTLE_ENDIAN)
                     (.asFloatBuffer))
        animations (:animations tex-set)]
    (into {} (map #(do [(:id %) (->anim-data % tex-coords)]) animations))))

(g/defnode AtlasNode
  (inherits project/ResourceNode)

  (property margin          types/NonNegativeInt (default 0))
  (property extrude-borders types/NonNegativeInt (default 0))

  (input animations Animation :array)
  (input outline g/Any :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)

  (output images           [Image]        :cached (g/fnk [animations] (vals (into {} (map (fn [img] [(:path img) img]) (mapcat :images animations))))))
  (output aabb             AABB           (g/fnk [texture-set-data] (let [img (:image texture-set-data)] (types/->AABB (Point3d. 0 0 0) (Point3d. (.getWidth img) (.getHeight img) 0)))))
  (output gpu-texture      g/Any          :cached (g/fnk [texture-set-data] (texture/image-texture (:image texture-set-data))))
  (output texture-set-data g/Any          :cached produce-texture-set-data)
  (output anim-data        g/Any          :cached produce-anim-data)
  (output outline          g/Any          :cached (g/fnk [node-id outline] {:node-id node-id :label "Atlas" :children outline :icon atlas-icon}))
  (output save-data        g/Any          :cached produce-save-data)
  (output scene            g/Any          :cached produce-scene))

(def ^:private atlas-animation-keys [:flip-horizontal :flip-vertical :fps :playback :id])

(defn- attach-atlas-image-nodes
  [graph-id base-node parent images src-label tgt-label]
  (for [[image img-node] (map (fn [img] [img (project/resolve-resource-node base-node img)]) images)
       :when img-node]
   (g/make-nodes
     graph-id
     [atlas-image [AtlasImage :path image]]
     (g/connect img-node    :content     atlas-image :src-image)
     (g/connect atlas-image :self        base-node   :nodes)
     (g/connect atlas-image src-label    parent      tgt-label)
     (g/connect atlas-image :outline     parent      :outline)
     (g/connect atlas-image :ddf-message parent      :img-ddf))))

(defn load-atlas [project self input]
  (let [atlas         (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))
        graph-id      (g/node->graph-id self)]
    (concat
      (g/set-property self :margin (:margin atlas))
      (g/set-property self :extrude-borders (:extrude-borders atlas))
      (attach-atlas-image-nodes graph-id self self (map :image (:images atlas)) :animation :animations)
      (for [anim (:animations atlas)
            :let [images (map :image (:images anim))]]
        (g/make-nodes
          (g/node->graph-id self)
          [atlas-anim [AtlasAnimation :flip-horizontal (:flip-horizontal anim) :flip-vertical (:flip-vertical anim) :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
          (g/connect atlas-anim :self        self :nodes)
          (g/connect atlas-anim :animation   self :animations)
          (g/connect atlas-anim :outline     self :outline)
          (g/connect atlas-anim :ddf-message self :anim-ddf)
          (attach-atlas-image-nodes graph-id self atlas-anim images :image :frames))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "atlas"
                                    :node-type AtlasNode
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))
