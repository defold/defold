(ns editor.atlas
  (:require [clojure.set :refer [difference union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [service.log :as log]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file :as file]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.grid :as grid]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass]
            [editor.camera :as c]
            [editor.scene-editor :as sceneed]
            [editor.image-node :as ein])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [com.dynamo.tile.proto Tile$Playback]
            [com.jogamp.opengl.util.awt TextRenderer]
            [dynamo.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [javax.media.opengl.glu GLU]
            [javax.vecmath Matrix4d]))

(vtx/defvertex engine-format-texture
  (vec3.float position)
  (vec2.short texcoord0 true))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(vtx/defvertex uv-only
  (vec2 uv))

(declare tex-outline-vertices)

(n/defnode AnimationGroupNode
  (inherits n/OutlineNode)

  (property images dp/ImageResourceList)
  (property id s/Str)
  (property fps             dp/NonNegativeInt (default 30))
  (property flip-horizontal s/Bool)
  (property flip-vertical   s/Bool)
  (property playback        AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (input images [ein/ImageResourceNode])

  (output animation Animation
    (fnk [this id images :- [Image] fps flip-horizontal flip-vertical playback]
      (->Animation id images fps flip-horizontal flip-vertical playback))))

(defnk produce-texture-packing :- TexturePacking
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (let [animations (concat animations (map tex/animation-from-image images))
        _          (assert (pos? (count animations)))
        images     (seq (into #{} (mapcat :images animations)))
        _          (assert (pos? (count images)))
        texture-packing (tex/pack-textures margin extrude-borders images)]
    (assoc texture-packing :animations animations)))

(defn build-textureset-animation-frame
  [texture-packing image]
  (let [coords (filter #(= (:path image) (:path %)) (:coords texture-packing))
        ; TODO: may fail due to #87253110 "Atlas texture should not contain multiple instances of the same image"
        ;_ (assert (= 1 (count coords)))
        coord (first coords)
        x-scale (/ 1.0 (.getWidth  (.packed-image texture-packing)))
        y-scale (/ 1.0 (.getHeight (.packed-image texture-packing)))
        u0 (* x-scale (+ (.x coord)))
        v0 (* y-scale (+ (.y coord)))
        u1 (* x-scale (+ (.x coord) (.width  coord)))
        v1 (* y-scale (+ (.y coord) (.height coord)))
        x0 (* -0.5 (.width  coord))
        y0 (* -0.5 (.height coord))
        x1 (*  0.5 (.width  coord))
        y1 (*  0.5 (.height coord))
        outline-vertices [[x0 y0 0 (g/to-short-uv u0) (g/to-short-uv v1)]
                          [x1 y0 0 (g/to-short-uv u1) (g/to-short-uv v1)]
                          [x1 y1 0 (g/to-short-uv u1) (g/to-short-uv v0)]
                          [x0 y1 0 (g/to-short-uv v0) (g/to-short-uv v0)]]]
    (t/map->TextureSetAnimationFrame
     {:image            image ; TODO: is this necessary?
      :outline-vertices outline-vertices
      :vertices         (mapv outline-vertices [0 1 2 0 2 3])
      :tex-coords       [[u0 v0] [u1 v1]]})))

(defn build-textureset-animation
  [texture-packing animation]
  (let [images (:images animation)
        width  (int (:width  (first images)))
        height (int (:height (first images)))
        frames (mapv (partial build-textureset-animation-frame texture-packing) images)]
    (-> (select-keys animation [:id :fps :flip-horizontal :flip-vertical :playback])
        (assoc :width width :height height :frames frames)
        t/map->TextureSetAnimation)))

(defnk produce-textureset :- TextureSet
  [this texture-packing :- TexturePacking]
  (let [animations (:animations texture-packing)
        textureset-animations (mapv (partial build-textureset-animation texture-packing) animations)]
    (t/map->TextureSet {:animations textureset-animations})))

(sm/defn build-atlas-image :- AtlasProto$AtlasImage
  [image :- Image]
  (.build (doto (AtlasProto$AtlasImage/newBuilder)
            (protobuf/set-if-present :image image (comp str :path)))))

(sm/defn build-atlas-animation :- AtlasProto$AtlasAnimation
  [animation :- Animation]
  (.build (doto (AtlasProto$AtlasAnimation/newBuilder)
            (.addAllImages           (map build-atlas-image (.images animation)))
            (.setId                  (.id animation))
            (.setFps                 (.fps animation))
            (protobuf/set-if-present :flip-horizontal animation)
            (protobuf/set-if-present :flip-vertical animation)
            (protobuf/set-if-present :playback animation (partial protobuf/val->pb-enum Tile$Playback)))))

(defnk get-text-format :- s/Str
  "get the text string for this node"
  [this images :- [Image] animations :- [Animation]]
  (pb->str
    (.build
         (doto (AtlasProto$Atlas/newBuilder)
             (.addAllImages           (map build-atlas-image images))
             (.addAllAnimations       (map build-atlas-animation animations))
             (protobuf/set-if-present :margin this)
             (protobuf/set-if-present :extrude-borders this)))))

(defnk save-atlas-file
  [this filename]
  (let [text (n/get-node-value this :text-format)]
    (file/write-file filename (.getBytes text))
    :ok))

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

(defnk produce-renderable :- RenderData
  [this texture-packing vertex-binding gpu-texture]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer texture-packing))}]
   pass/transparent
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-texture-packing ctx gl texture-packing vertex-binding gpu-texture))}]})

(defnk produce-renderable-vertex-buffer
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

(defnk produce-outline-vertex-buffer
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

(n/defnode AtlasRender
  (input gpu-texture s/Any)
  (input texture-packing s/Any)

  (output vertex-buffer s/Any         :cached produce-renderable-vertex-buffer)
  (output outline-vertex-buffer s/Any :cached produce-outline-vertex-buffer)
  (output vertex-binding s/Any        :cached (fnk [vertex-buffer] (vtx/use-with vertex-buffer atlas-shader)))
  (output renderable RenderData       produce-renderable))

(defn build-animation
  [anim begin]
  (let [start begin
        end   (+ begin (count (:frames anim)))]
    (.build
      (doto (TextureSetProto$TextureSetAnimation/newBuilder)
         (.setId                  (:id anim))
         (.setWidth               (int (:width  anim)))
         (.setHeight              (int (:height anim)))
         (.setStart               (int start))
         (.setEnd                 (int end))
         (protobuf/set-if-present :playback anim (partial protobuf/val->pb-enum Tile$Playback))
         (protobuf/set-if-present :fps anim)
         (protobuf/set-if-present :flip-horizontal anim)
         (protobuf/set-if-present :flip-vertical anim)
         (protobuf/set-if-present :is-animation anim)))))

(defn build-animations
  [start-idx animations]
  (let [frame-counts     (map #(count (:frames %)) animations)
        animation-starts (butlast (reductions + start-idx frame-counts))]
    (map build-animation animations animation-starts)))

(defn summarize-frames [key vbuf-factory-fn frames]
  (let [counts (map #(count (get % key)) frames)
        starts (reductions + 0 counts)
        total  (last starts)
        starts (butlast starts)
        vbuf   (vbuf-factory-fn total)]
    (doseq [frame frames
            vtx (get frame key)]
      (conj! vbuf vtx))
    {:starts (map int starts) :counts (map int counts) :vbuf (persistent! vbuf)}))

(defn texturesetc-protocol-buffer
  [texture-name textureset]
  (let [animations             (remove #(empty? (:frames %)) (:animations textureset))
        frames                 (mapcat :frames animations)
        vertex-summary         (summarize-frames :vertices         ->engine-format-texture frames)
        outline-vertex-summary (summarize-frames :outline-vertices ->engine-format-texture frames)
        tex-coord-summary      (summarize-frames :tex-coords       ->uv-only               frames)]
    (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               texture-name)
            (.setTexCoords             (byte-pack (:vbuf tex-coord-summary)))
            (.addAllAnimations         (build-animations 0 animations))

            (.addAllVertexStart        (:starts vertex-summary))
            (.addAllVertexCount        (:counts vertex-summary))
            (.setVertices              (byte-pack (:vbuf vertex-summary)))

            (.addAllOutlineVertexStart (:starts outline-vertex-summary))
            (.addAllOutlineVertexCount (:counts outline-vertex-summary))
            (.setOutlineVertices       (byte-pack (:vbuf outline-vertex-summary)))

            (.setTileCount             (int 0))))))

(defnk compile-texturesetc :- s/Bool
  [this g project textureset :- TextureSet]
  (file/write-file (:textureset-filename this)
    (.toByteArray (texturesetc-protocol-buffer (:texture-name this) textureset)))
  :ok)

(defn- texturec-protocol-buffer
  [engine-format]
  (s/validate EngineFormatTexture engine-format)
  (.build (doto (Graphics$TextureImage/newBuilder)
            (.addAlternatives
              (doto (Graphics$TextureImage$Image/newBuilder)
                (.setWidth           (.width engine-format))
                (.setHeight          (.height engine-format))
                (.setOriginalWidth   (.original-width engine-format))
                (.setOriginalHeight  (.original-height engine-format))
                (.setFormat          (.format engine-format))
                (.setData            (byte-pack (.data engine-format)))
                (.addAllMipMapOffset (.mipmap-offsets engine-format))
                (.addAllMipMapSize   (.mipmap-sizes engine-format))))
            (.setType            (Graphics$TextureImage$Type/TYPE_2D))
            (.setCount           1))))

(defnk compile-texturec :- s/Bool
  [this g project packed-image :- BufferedImage]
  (file/write-file (:texture-filename this)
    (.toByteArray (texturec-protocol-buffer (tex/->engine-format packed-image))))
  :ok)

(n/defnode TextureSave
  (input textureset   TextureSet)
  (input packed-image BufferedImage)

  (property texture-filename    s/Str (default ""))
  (property texture-name        s/Str)
  (property textureset-filename s/Str (default ""))

  (output   texturec    s/Any :on-update compile-texturec)
  (output   texturesetc s/Any :on-update compile-texturesetc))

(defn broadcast-event [this event]
  (let [[controllers] (n/get-node-inputs this :controllers)]
    (doseq [controller controllers]
      (t/process-one-event controller event))))

(n/defnode BroadcastController
  (input controllers [s/Any])
  (on :mouse-down (broadcast-event self event))
  (on :mouse-up (broadcast-event self event))
  (on :mouse-double-click (broadcast-event self event))
  (on :mouse-enter (broadcast-event self event))
  (on :mouse-exit (broadcast-event self event))
  (on :mouse-hover (broadcast-event self event))
  (on :mouse-move (broadcast-event self event))
  (on :mouse-wheel (broadcast-event self event))
  (on :key-down (broadcast-event self event))
  (on :key-up (broadcast-event self event)))

(defn find-resource-nodes [project exts]
  (let [all-resource-nodes (filter (fn [node] (let [filename (:filename node)]
                                                (and filename (contains? exts (t/extension filename))))) (map first (ds/sources-of project :nodes)))
        filenames (map (fn [node]
                         (let [filename (:filename node)]
                           (str "/" (t/local-path filename)))) all-resource-nodes)]
    (zipmap filenames all-resource-nodes)))

(defn construct-ancillary-nodes
  [self project input]
  (let [atlas (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))
        img-nodes (find-resource-nodes project #{"png" "jpb"})]
    (ds/set-property self :margin (:margin atlas))
    (ds/set-property self :extrude-borders (:extrude-borders atlas))
    (doseq [anim (:animations atlas)
            :let [anim-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (select-keys anim [:flip-horizontal :flip-vertical :fps :playback :id]))))
                  images (mapv :image (:images anim))]]
      (ds/set-property anim-node :images images)
      (ds/connect anim-node :animation self :animations)
      (ds/connect anim-node :tree      self :children)
      (doseq [image images]
        (when-let [img-node (get img-nodes image)]
          (ds/connect img-node :content anim-node :images)
          )))
    (ds/set-property self :images (mapv :image (:images atlas)))
    self))

(n/defnode AtlasNode
  "This node represents an actual Atlas. It accepts a collection
   of images and animations. It emits a packed texture-packing.

   Inputs:
   images `[dynamo.types/Image]` - A collection of images that will be packed into the atlas.
   animations `[dynamo.types/Animation]` - A collection of animations that will be packed into the atlas.

   Properties:
   margin - Integer, must be zero or greater. The number of pixels of transparent space to leave between textures.
   extrude-borders - Integer, must be zero or greater. The number of pixels for which the outer edge of each texture will be duplicated.

   The margin fits outside the extruded border.

   Outputs
   aabb `dynamo.types.AABB` - The AABB of the packed texture, in pixel space.
   gpu-texture `Texture` - A wrapper for the BufferedImage with the actual pixels. Conforms to the right protocols so you can directly use this in rendering.
   text-format `String` - A saveable representation of the atlas, its animations, and images. Built as a text-formatted protocol buffer.
   texture-packing `dynamo.types/TexturePacking` - A data structure with full access to the original image bounds, their coordinates in the packed image, the BufferedImage, and outline coordinates.
   packed-image `BufferedImage` - BufferedImage instance with the actual pixels.
   textureset `dynamo.types/TextureSet` - A data structure that logically mirrors the texturesetc protocol buffer format."
  (inherits n/OutlineNode)
  (inherits n/ResourceNode)
  (inherits n/Saveable)

  (property images dp/ImageResourceList (visible false))

  (property margin          dp/NonNegativeInt (default 0) (visible true))
  (property extrude-borders dp/NonNegativeInt (default 0) (visible true))
  (property filename (s/protocol PathManipulation) (visible false))

  (input animations [Animation])
  (input images [ein/ImageResourceNode])

  (output aabb            AABB               (fnk [texture-packing] (g/rect->aabb (:aabb texture-packing))))
  (output gpu-texture     s/Any      :cached (fnk [packed-image] (texture/image-texture packed-image)))
  (output save            s/Keyword          save-atlas-file)
  (output text-format     s/Str              get-text-format)
  (output texture-packing TexturePacking :cached :substitute-value (tex/blank-texture-packing) produce-texture-packing)
  (output packed-image    BufferedImage  :cached (fnk [texture-packing] (:packed-image texture-packing)))
  (output textureset      TextureSet     :cached produce-textureset)

  (on :load
      (let [project (:project event)
            input (:filename self)
            atlas (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))
            img-nodes (find-resource-nodes project #{"png" "jpb"})]
        (ds/set-property self :margin (:margin atlas))
        (ds/set-property self :extrude-borders (:extrude-borders atlas))
        (doseq [anim (:animations atlas)
                :let [anim-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (select-keys anim [:flip-horizontal :flip-vertical :fps :playback :id]))))
                      images (mapv :image (:images anim))]]
          (ds/set-property anim-node :images images)
          (ds/connect anim-node :animation self :animations)
          (ds/connect anim-node :tree      self :children)
          (doseq [image images]
            (when-let [img-node (get img-nodes image)]
              (ds/connect img-node :content anim-node :images)
              )))
        (let [images (mapv :image (:images atlas))]
          (prn "imgs" images)
          (ds/set-property self :images images))
        self))

  (on :unload
      (doseq [[animation-group _] (ds/sources-of self :animations)]
        (ds/delete animation-group))))

(defn construct-atlas-editor
  [project-node atlas-node]
  (let [editor (n/construct sceneed/SceneEditor)]
    (ds/in (ds/add editor)
           (let [atlas-render (ds/add (n/construct AtlasRender))
                 renderer     (ds/add (n/construct sceneed/SceneRenderer))
                 background   (ds/add (n/construct background/Gradient))
                 grid         (ds/add (n/construct grid/Grid))
                 camera       (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
             (ds/connect background   :renderable      renderer     :renderables)
             (ds/connect grid         :renderable      renderer     :renderables)
             (ds/connect camera       :camera          grid         :camera)
             (ds/connect camera       :camera          renderer     :camera)
             (ds/connect camera       :self            editor       :controller)
             (ds/connect editor       :viewport        camera       :viewport)
             (ds/connect editor       :viewport        renderer     :viewport)
             (ds/connect editor       :drawable        renderer     :drawable)
             (ds/connect renderer     :frame           editor       :frame)

             (ds/connect atlas-node   :texture-packing atlas-render :texture-packing)
             (ds/connect atlas-node   :gpu-texture     atlas-render :gpu-texture)
             (ds/connect atlas-render :renderable      renderer     :renderables)
             )
           editor)))
