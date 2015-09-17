(ns editor.atlas
  (:require [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.image :as image]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.texture :as tex]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.scene :as scene]
            [internal.render.pass :as pass])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
           [com.dynamo.tile.proto Tile$Playback]
           [com.jogamp.opengl.util.awt TextRenderer]
           [com.google.protobuf ByteString]
           [editor.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]
           [java.nio ByteBuffer ByteOrder FloatBuffer]))

(set! *warn-on-reflection* true)

(def atlas-icon "icons/32/Icons_13-Atlas.png")
(def animation-icon "icons/32/Icons_24-AT-Animation.png")
(def image-icon "icons/32/Icons_25-AT-Image.png")

(defn render-overlay
  [^GL2 gl width height]
  (scene/overlay-text gl (format "Size: %dx%d" width height) 12.0 -22.0))

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

; TODO - macro of this
(def atlas-shader (shader/make-shader ::atlas-shader pos-uv-vert pos-uv-frag))

(defn render-texture-set
  [gl vertex-binding gpu-texture]
  (gl/with-gl-bindings gl [gpu-texture atlas-shader vertex-binding]
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
                         :flip-horizontal false
                         :flip-vertical   false
                         :playback        :playback-once-forward}))

(g/defnode AtlasImage
  (input src-resource (g/protocol workspace/Resource))
  (input src-image BufferedImage)
  (output path g/Str (g/fnk [src-resource] (workspace/proj-path src-resource)))
  (output image Image (g/fnk [path ^BufferedImage src-image] (Image. path src-image (.getWidth src-image) (.getHeight src-image))))
  (output animation Animation (g/fnk [image] (image->animation image)))
  (output outline g/Any (g/fnk [_node-id path] {:node-id _node-id
                                                :label (format "%s - %s" (path->id path) path)
                                                :icon image-icon}))
  (output ddf-message g/Any :cached (g/fnk [path] {:image path})))

(g/defnk produce-anim-ddf [id fps flip-horizontal flip-vertical playback img-ddf]
  {:id id
   :fps fps
   :flip-horizontal flip-horizontal
   :flip-vertical flip-vertical
   :playback playback
   :images img-ddf})

(g/defnode AtlasAnimation
  (property id  g/Str)
  (property fps g/Int
            (default 30)
            (validate
             (g/fnk [fps]
                    (when (neg? fps)
                      (g/error-info "FPS must be greater than or equal to zero")))))
  (property flip-horizontal g/Bool)
  (property flip-vertical   g/Bool)
  (property playback        types/AnimationPlayback
            (dynamic edit-type (g/always
                                 (let [options (protobuf/enum-values Tile$Playback)]
                                   {:type :choicebox
                                    :options (zipmap (map first options)
                                                     (map (comp :display-name second) options))}))))

  (input frames Image :array)
  (input outline g/Any :array)
  (input img-ddf g/Any :array)

  (output animation Animation (g/fnk [this id frames fps flip-horizontal flip-vertical playback]
                                     (types/->Animation id frames fps flip-horizontal flip-vertical playback)))
  (output outline g/Any (g/fnk [_node-id id outline] {:node-id _node-id :label id :children outline :icon animation-icon}))
  (output ddf-message g/Any :cached produce-anim-ddf))

(g/defnk produce-save-data [resource margin inner-padding extrude-borders img-ddf anim-ddf]
  {:resource resource
   :content (let [m {:margin margin
                     :inner-padding inner-padding
                     :extrude-borders extrude-borders
                     :images img-ddf
                     :animations anim-ddf}]
              (protobuf/map->str AtlasProto$Atlas m))})

; TODO - fix real profiles
(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(defn- build-texture [self basis resource dep-resources user-data]
  {:resource resource :content (tex-gen/->bytes (:image user-data) test-profile)})

(defn- build-texture-set [self basis resource dep-resources user-data]
  (let [tex-set (assoc (:proto user-data) :texture (workspace/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes TextureSetProto$TextureSet tex-set)}))

(g/defnk produce-build-targets [_node-id project-id resource texture-set-data save-data]
  (let [workspace        (project/workspace project-id)
        texture-type     (workspace/get-resource-type workspace "texture")
        texture-resource (workspace/make-memory-resource workspace texture-type (:content save-data))
        texture-target   {:node-id   _node-id
                          :resource  (workspace/make-build-resource texture-resource)
                          :build-fn  build-texture
                          :user-data {:image (:image texture-set-data)}}]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture-set
      :user-data {:proto (:texture-set texture-set-data)}
      :deps [texture-target]}]))

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
  [_node-id texture-set-data aabb gpu-texture]
  (let [^BufferedImage img (:image texture-set-data)
        width (.getWidth img)
        height (.getHeight img)
        vertex-buffer (gen-renderable-vertex-buffer width height)
        vertex-binding (vtx/use-with _node-id vertex-buffer atlas-shader)]
    {:aabb aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (let [pass (:pass render-args)]
                                 (cond
                                   (= pass pass/overlay)     (render-overlay gl width height)
                                   (= pass pass/transparent) (render-texture-set gl vertex-binding gpu-texture))))
                  :passes [pass/overlay pass/transparent]}}))

(g/defnk produce-texture-set-data [animations images margin inner-padding extrude-borders]
  (texture-set-gen/->texture-set-data animations images margin inner-padding extrude-borders))

(defn- ->uv-vertex [vert-index ^FloatBuffer tex-coords]
  (let [index (* vert-index 2)]
    [(.get tex-coords ^int index) (.get tex-coords ^int (inc index))]))

(defn- ->uv-quad [quad-index tex-coords]
  (let [offset (* quad-index 4)]
    (mapv #(->uv-vertex (+ offset %) tex-coords) (range 4))))

(defn- ->anim-frame [frame-index tex-coords]
  {:tex-coords (->uv-quad frame-index tex-coords)})

(defn- ->anim-data [anim tex-coords uv-transforms]
  {:width (:width anim)
   :height (:height anim)
   :frames (mapv #(->anim-frame % tex-coords) (range (:start anim) (:end anim)))
   :uv-transforms (subvec uv-transforms (:start anim) (:end anim))})

(g/defnk produce-anim-data [texture-set-data]
  (let [tex-set (:texture-set texture-set-data)
        tex-coords (-> ^ByteString (:tex-coords tex-set)
                     (.asReadOnlyByteBuffer)
                     (.order ByteOrder/LITTLE_ENDIAN)
                     (.asFloatBuffer))
        uv-transforms (:uv-transforms texture-set-data)
        animations (:animations tex-set)]
    (into {} (map #(do [(:id %) (->anim-data % tex-coords uv-transforms)]) animations))))

(g/defnode AtlasNode
  (inherits project/ResourceNode)

  (property margin g/Int
            (default 0)
            (validate
             (g/fnk [margin]
                    (when (neg? margin)
                      (g/error-info "Margin must be greater than or equal to zero")))))
  (property inner-padding g/Int
            (default 0)
            (validate
             (g/fnk [inner-padding]
                    (when (neg? inner-padding)
                      (g/error-info "Inner padding must be greater than or equal to zero")))))
  (property extrude-borders g/Int
            (default 0)
            (validate
               (g/fnk [extrude-borders]
                      (when (neg? extrude-borders)
                        (g/error-info "Extrude borders must be greater than or equal to zero")))))

  (input animations Animation :array)
  (input outline g/Any :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)

  (output images           [Image]        :cached (g/fnk [animations] (vals (into {} (map (fn [img] [(:path img) img]) (mapcat :images animations))))))
  (output aabb             AABB           (g/fnk [texture-set-data] (let [^BufferedImage img (:image texture-set-data)] (types/->AABB (Point3d. 0 0 0) (Point3d. (.getWidth img) (.getHeight img) 0)))))
  (output gpu-texture      g/Any          :cached (g/fnk [_node-id texture-set-data] (texture/image-texture _node-id (:image texture-set-data))))
  (output texture-set-data g/Any          :cached produce-texture-set-data)
  (output anim-data        g/Any          :cached produce-anim-data)
  (output outline          g/Any          :cached (g/fnk [_node-id outline] {:node-id _node-id :label "Atlas" :children outline :icon atlas-icon}))
  (output save-data        g/Any          :cached produce-save-data)
  (output build-targets    g/Any          :cached produce-build-targets)
  (output scene            g/Any          :cached produce-scene))

(def ^:private atlas-animation-keys [:flip-horizontal :flip-vertical :fps :playback :id])

(defn- attach-atlas-image-nodes
  [graph-id base-node parent images src-label tgt-label]
  (for [image images]
    (g/make-nodes
      graph-id
      [atlas-image [AtlasImage]]
      (project/connect-resource-node (project/get-project base-node) image atlas-image [[:content :src-image]
                                                                                        [:resource :src-resource]])
      (g/connect atlas-image :_node-id         base-node   :nodes)
      (g/connect atlas-image src-label    parent      tgt-label)
      (g/connect atlas-image :outline     parent      :outline)
      (g/connect atlas-image :ddf-message parent      :img-ddf))))

(defn add-images [atlas-node img-resources]
  (attach-atlas-image-nodes (g/node-id->graph-id atlas-node) atlas-node atlas-node img-resources :animation :animations))

(defn load-atlas [project self input]
  (let [atlas         (protobuf/read-text AtlasProto$Atlas input)
        graph-id      (g/node-id->graph-id self)]
    (concat
      (g/set-property self :margin (:margin atlas))
      (g/set-property self :inner-padding (:inner-padding atlas))
      (g/set-property self :extrude-borders (:extrude-borders atlas))
      (attach-atlas-image-nodes graph-id self self (map :image (:images atlas)) :animation :animations)
      (for [anim (:animations atlas)
            :let [images (map :image (:images anim))]]
        (g/make-nodes
          (g/node-id->graph-id self)
          [atlas-anim [AtlasAnimation :flip-horizontal (not= 0 (:flip-horizontal anim)) :flip-vertical (not= 0 (:flip-vertical anim))
                       :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
          (g/connect atlas-anim :_node-id         self :nodes)
          (g/connect atlas-anim :animation   self :animations)
          (g/connect atlas-anim :outline     self :outline)
          (g/connect atlas-anim :ddf-message self :anim-ddf)
          (attach-atlas-image-nodes graph-id self atlas-anim images :image :frames))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "atlas"
                                    :label "Atlas"
                                    :build-ext "texturesetc"
                                    :node-type AtlasNode
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))
