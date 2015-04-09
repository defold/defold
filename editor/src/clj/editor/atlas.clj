(ns editor.atlas
  (:require [clojure.string :as str]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.image :refer :all]
            [dynamo.property :as dp]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
           [com.dynamo.tile.proto Tile$Playback]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d]))

(def atlas-icon "icons/images.png")
(def animation-icon "icons/film.png")
(def image-icon "icons/image.png")

(vtx/defvertex engine-format-texture
  (vec3.float position)
  (vec2.short texcoord0 true))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(vtx/defvertex uv-only
  (vec2 uv))

(g/defnk produce-texture-packing :- TexturePacking
  [images :- [Image] margin extrude-borders]
  (tex/pack-textures margin extrude-borders images))

(defn summarize-frame-data [key vbuf-factory-fn frame-data]
  (let [counts (map #(count (get % key)) frame-data)
        starts (reductions + 0 counts)
        total  (last starts)
        starts (butlast starts)
        vbuf   (vbuf-factory-fn total)]
    (doseq [frame frame-data
            vtx (get frame key)]
      (conj! vbuf vtx))
    {:starts (map int starts) :counts (map int counts) :vbuf (persistent! vbuf)}))

(defn animation-frame-data
  [^TexturePacking texture-packing image]
  (let [coords (filter #(= (:path image) (:path %)) (:coords texture-packing))
        ^Rect coord (first coords)
        packed-image ^BufferedImage (.packed-image texture-packing)
        x-scale (/ 1.0 (.getWidth  packed-image))
        y-scale (/ 1.0 (.getHeight packed-image))
        u0 (* x-scale (+ (.x coord)))
        v0 (* y-scale (+ (.y coord)))
        u1 (* x-scale (+ (.x coord) (.width  coord)))
        v1 (* y-scale (+ (.y coord) (.height coord)))
        x0 (* -0.5 (.width  coord))
        y0 (* -0.5 (.height coord))
        x1 (*  0.5 (.width  coord))
        y1 (*  0.5 (.height coord))
        outline-vertices [[x0 y0 0 (geom/to-short-uv u0) (geom/to-short-uv v1)]
                          [x1 y0 0 (geom/to-short-uv u1) (geom/to-short-uv v1)]
                          [x1 y1 0 (geom/to-short-uv u1) (geom/to-short-uv v0)]
                          [x0 y1 0 (geom/to-short-uv u0) (geom/to-short-uv v0)]]]
    {:image            image ; TODO: is this necessary?
     :outline-vertices outline-vertices
     :vertices         (mapv outline-vertices [0 1 2 0 2 3])
     :tex-coords       [[u0 v0] [u1 v1]]}))

(defn build-textureset-animation
  [animation]
  (let [images (:images animation)
        width  (int (:width  (first images)))
        height (int (:height (first images)))]
    (-> (select-keys animation [:id :fps :flip-horizontal :flip-vertical :playback])
        (assoc :width width :height height)
        t/map->TextureSetAnimation)))

(g/defnk produce-textureset :- TextureSet
  [texture-packing :- TexturePacking animations]
  (let [animations             (remove #(empty? (:images %)) animations)
        animations-images      (for [a animations i (:images a)] [a i])
        images                 (mapcat :images animations)
        frame-data             (map (partial animation-frame-data texture-packing) images)
        vertex-summary         (summarize-frame-data :vertices         ->engine-format-texture frame-data)
        outline-vertex-summary (summarize-frame-data :outline-vertices ->engine-format-texture frame-data)
        tex-coord-summary      (summarize-frame-data :tex-coords       ->uv-only               frame-data)
        frames                 (map t/->TextureSetAnimationFrame
                                 images
                                 (:starts vertex-summary)
                                 (:counts vertex-summary)
                                 (:starts outline-vertex-summary)
                                 (:counts outline-vertex-summary)
                                 (:starts tex-coord-summary)
                                 (:counts tex-coord-summary))
        animation-frames       (partition-by first (map (fn [[a i] f] [a f]) animations-images frames))
        textureset-animations  (map build-textureset-animation animations)
        textureset-animations  (map (fn [a aframes] (assoc a :frames (mapv second aframes))) textureset-animations animation-frames)]
    (t/map->TextureSet {:animations       (reduce (fn [m a] (assoc m (:id a) a)) {} textureset-animations)
                        :vertices         (:vbuf vertex-summary)
                        :outline-vertices (:vbuf outline-vertex-summary)
                        :tex-coords       (:vbuf tex-coord-summary)})))

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer texture-packing]
  (let [image ^BufferedImage (.packed-image texture-packing)]
    (gl/overlay ctx gl text-renderer (format "Size: %dx%d" (.getWidth image) (.getHeight image)) 12.0 -22.0 1.0 1.0)))

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

(defn render-texture-packing
  [ctx gl texture-packing vertex-binding gpu-texture]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (count (:coords texture-packing))))))

(defn render-quad
  [ctx gl texture-packing vertex-binding gpu-texture i]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES (* 6 i) 6)))

(g/defnk produce-renderable :- RenderData
  [this texture-packing vertex-binding gpu-texture]
  {pass/overlay
   [{:world-transform geom/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer texture-packing))}]
   pass/transparent
   [{:world-transform geom/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-texture-packing ctx gl texture-packing vertex-binding gpu-texture))}]})

(g/defnk produce-renderable-vertex-buffer
  [[:texture-packing aabb coords]]
  (let [vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width aabb))
        y-scale    (/ 1.0 (.height aabb))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height aabb) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height aabb) (+ (.y coord) h))
            u0 (* x0 x-scale)
            v0 (* y0 y-scale)
            u1 (* x1 x-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [x0 y0 0 1 u0 (- 1 v0)])
          (conj! [x0 y1 0 1 u0 (- 1 v1)])
          (conj! [x1 y1 0 1 u1 (- 1 v1)])

          (conj! [x1 y1 0 1 u1 (- 1 v1)])
          (conj! [x1 y0 0 1 u1 (- 1 v0)])
          (conj! [x0 y0 0 1 u0 (- 1 v0)]))))
    (persistent! vbuf)))

(g/defnk produce-outline-vertex-buffer
  [[:texture-packing aabb coords]]
  (prn coords)
  (let [vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width aabb))
        y-scale    (/ 1.0 (.height aabb))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height aabb) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height aabb) (+ y0 h))
            u0 (* x0 x-scale)
            v0 (* y0 y-scale)
            u1 (* x1 x-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [x0 y0 0 1 u0 (- 1 v0)])
          (conj! [x0 y1 0 1 u0 (- 1 v1)])
          (conj! [x1 y1 0 1 u1 (- 1 v1)])
          (conj! [x1 y0 0 1 u1 (- 1 v0)]))))
    (persistent! vbuf)))

(g/defnode AtlasRender
  (input gpu-texture t/Any)
  (input texture-packing t/Any)

  (output vertex-buffer t/Any         :cached produce-renderable-vertex-buffer)
  (output outline-vertex-buffer t/Any :cached produce-outline-vertex-buffer)
  (output vertex-binding t/Any        :cached (g/fnk [vertex-buffer] (vtx/use-with vertex-buffer atlas-shader)))
  (output renderable RenderData       produce-renderable))

(defn- path->id [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn image->animation [image]
  (map->Animation {:id              (path->id (:path image))
                   :images          [image]
                   :fps             30
                   :flip-horizontal 0
                   :flip-vertical   0
                   :playback        :PLAYBACK_ONCE_FORWARD}))

(g/defnode AtlasImage
  (property path t/Str)
  (input src-image BufferedImage)
  (output image Image (g/fnk [path src-image] (Image. path src-image (.getWidth src-image) (.getHeight src-image))))
  (output animation Animation (g/fnk [image] (image->animation image)))
  (output outline t/Any (g/fnk [self path] {:self self :label (path->id path) :icon image-icon})))

(g/defnode AtlasAnimation
  (property id t/Str)
  (property fps             dp/NonNegativeInt (default 30))
  (property flip-horizontal t/Bool)
  (property flip-vertical   t/Bool)
  (property playback        AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (input frames [Image])
  (input outline [t/Any])

  (output animation Animation (g/fnk [this id frames :- [Image] fps flip-horizontal flip-vertical playback]
                                     (->Animation id frames fps flip-horizontal flip-vertical playback)))
  (output outline t/Any (g/fnk [self id outline] {:self self :label id :children outline :icon animation-icon})))

(g/defnode AtlasNode
  (inherits project/ResourceNode)

  (property margin          dp/NonNegativeInt (default 0))
  (property extrude-borders dp/NonNegativeInt (default 0))

  (input animations [Animation])
  (input outline [t/Any])

  (output images          [Image]        :cached (g/fnk [animations] (vals (into {} (map (fn [img] [(:path img) img]) (mapcat :images animations))))))
  (output aabb            AABB           (g/fnk [texture-packing] (geom/rect->aabb (:aabb texture-packing))))
  (output gpu-texture     t/Any          :cached (g/fnk [packed-image] (texture/image-texture packed-image)))
  (output texture-packing TexturePacking :cached :substitute-value (tex/blank-texture-packing) produce-texture-packing)
  (output packed-image    BufferedImage  :cached (g/fnk [texture-packing] (:packed-image texture-packing)))
  (output textureset      TextureSet     :cached produce-textureset)
  (output outline         t/Any          :cached (g/fnk [self outline] {:self self :label "Atlas" :children outline :icon atlas-icon})))

(def ^:private atlas-animation-keys [:flip-horizontal :flip-vertical :fps :playback :id])

(defn- attach-atlas-image-nodes
  [graph-id parent img-nodes images]
  (for [image images
        :when (get img-nodes image)
        :let [img-node (get img-nodes image)]]
    (g/make-nodes
      graph-id
      [atlas-image [AtlasImage :path image]]
      (g/connect (get img-nodes image) :content   atlas-image :src-image)
      (g/connect atlas-image           :animation parent      :animations)
      (g/connect atlas-image           :outline   parent      :outline))))

(defn load-atlas [project self input]
  (let [atlas         (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))
        img-resources (project/find-resources project "**.{png,jpg}")
        img-nodes     (into {} (map (fn [[resource node]] [(workspace/path resource) node]) img-resources))
        graph-id      (g/nref->gid (g/node-id self))]
    (concat
      ;; TODO Bug, protobuf defaults should be preserved
      (g/set-property self :margin (get atlas :margin 0))
      (g/set-property self :extrude-borders (get atlas :extrude-borders 0))
      (attach-atlas-image-nodes graph-id self img-nodes (mapv #(subs (:image %) 1) (:images atlas)))
      (for [anim (:animations atlas)
            :let [images (mapv :image (:images anim))]]
        (g/make-nodes
          (g/nref->gid (g/node-id self))
          [atlas-anim [AtlasAnimation :flip-horizontal (:flip-horizontal anim) :flip-vertical (:flip-vertical anim) :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
          (g/connect atlas-anim :animation self :animations)
          (g/connect atlas-anim :outline   self :outline)
          (attach-atlas-image-nodes graph-id atlas-anim img-nodes images))))))

(defn setup-rendering [self view]
  (let [renderer (t/lookup view :renderer)
        camera (t/lookup view :camera)]
    (g/make-nodes
      (g/nref->gid (g/node-id view))
      [atlas-render AtlasRender]
      (g/connect self         :texture-packing atlas-render :texture-packing)
      (g/connect self         :gpu-texture     atlas-render :gpu-texture)
      (g/connect atlas-render :renderable      renderer     :renderables)
      (g/connect self         :aabb            camera       :aabb))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "atlas"
                                    :node-type AtlasNode
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene]
                                    :view-fns {:scene {:setup-view-fn (fn [self view] (scene/setup-view view :grid true))
                                                       :setup-rendering-fn setup-rendering}}))
