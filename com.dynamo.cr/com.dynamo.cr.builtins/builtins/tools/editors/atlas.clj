(ns editors.atlas
  (:require [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.editors :as ed]
            [dynamo.file :as file]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [dynamo.geom :as g :refer [to-short-uv]]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.grid :as grid]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.selection :as sel]
            [dynamo.system :as ds]
            [dynamo.texture :refer :all]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [com.dynamo.tile.proto Tile$Playback]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer IntBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture AABB]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [javax.vecmath Matrix4d]))

(def integers (iterate (comp int inc) (int 0)))

(vtx/defvertex engine-format-texture
  (vec3.float position)
  (vec2.short texcoord0 true))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(vtx/defvertex uv-only
  (vec2 uv))

(declare tex-outline-vertices)

(defnk produce-animation :- Animation
  [this images :- [Image] id fps flip-horizontal flip-vertical playback]
  (->Animation id images fps flip-horizontal flip-vertical playback))

(n/defnode AnimationGroupNode
  (inherits n/OutlineNode)
  (inherits AnimationBehavior)

  (output animation Animation produce-animation)

  (property id s/Str))

(sm/defn ^:private consolidate :- [Image]
  [images :- [Image] containers :- [{:images [Image]}]]
  (seq (apply union
              (into #{} images)
              (map #(into #{} (:images %)) containers))))

(defnk produce-textureset :- TextureSet
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (-> (pack-textures margin extrude-borders (consolidate images animations))
    (assoc :animations animations)))

(defnk produce-aabb :- AABB
  [this]
  (let [textureset (n/get-node-value this :textureset)]
    (g/rect->aabb (:aabb textureset))))

(n/defnode AtlasProperties
  (inherits n/DirtyTracking)

  (input assets [OutlineItem])
  (input images [Image])
  (input animations [Animation])

  (property margin          dp/NonNegativeInt (default 0))
  (property extrude-borders dp/NonNegativeInt (default 0))

  (output textureset TextureSet :cached :substitute-value (blank-textureset) produce-textureset)
  (output aabb       AABB               produce-aabb))

(sm/defn build-atlas-image :- AtlasProto$AtlasImage
  [image :- Image]
  (.build (doto (AtlasProto$AtlasImage/newBuilder)
            (.setImage (str (.path image))))))

(sm/defn build-atlas-animation :- AtlasProto$AtlasAnimation
  [animation :- Animation]
  (.build (doto (AtlasProto$AtlasAnimation/newBuilder)
            (.addAllImages           (map build-atlas-image (.images animation)))
            (.setId                  (.id animation))
            (.setFps                 (.fps animation))
            (protobuf/set-if-present :flip-horizontal animation)
            (protobuf/set-if-present :flip-vertical animation)
            (.setPlayback            (protobuf/val->pb-enum Tile$Playback (.playback animation))))))

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

(n/defnode AtlasSave
  (inherits n/ResourceNode)
  (inherits n/Saveable)

  (property filename (s/protocol PathManipulation) (visible false))

  (output save s/Keyword save-atlas-file)
  (output text-format s/Str get-text-format))

(defn vertex-starts [n-vertices] (take n-vertices (take-nth 6 integers)))
(defn vertex-counts [n-vertices] (take n-vertices (repeat (int 6))))

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer this]
  (let [textureset ^TextureSet (n/get-node-value this :textureset)
        image      ^BufferedImage (.packed-image textureset)]
    (gl/overlay ctx gl text-renderer (format "Size: %dx%d" (.getWidth image) (.getHeight image)) 12.0 -22.0 1.0 1.0)))

(defnk produce-gpu-texture
  [this gl textureset]
  (texture/image-texture gl (:packed-image textureset)))

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

(defnk produce-shader :- s/Int
  [this gl ctx]
  (shader/make-shader ctx gl pos-uv-vert pos-uv-frag))

(defn render-textureset
  [ctx gl this]
  (gl/do-gl [this            (assoc this :gl gl :ctx ctx)
             textureset      (n/get-node-value this :textureset)
             texture         (n/get-node-value this :gpu-texture)
             shader          (n/get-node-value this :shader)
             vbuf            (n/get-node-value this :vertex-buffer)
             vertex-binding  (vtx/use-with gl vbuf shader)]
    (shader/set-uniform shader "texture" (texture/texture-unit-index texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (count (:coords textureset))))))

(defnk produce-renderable :- RenderData
  [this textureset]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer this))}]
   pass/transparent
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-textureset ctx gl this))}]})

(defnk produce-renderable-vertex-buffer
  [this gl textureset]
  (let [shader     (n/get-node-value this :shader)
        bounds     (:aabb textureset)
        coords     (:coords textureset)
        vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width bounds))
        y-scale    (/ 1.0 (.height bounds))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height bounds) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height bounds) (+ (.y coord) h))
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
  [this gl]
  (let [textureset (n/get-node-value this :textureset)
        shader     (n/get-node-value this :shader)
        bounds     (:aabb textureset)
        coords     (:coords textureset)
        vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width bounds))
        y-scale    (/ 1.0 (.height bounds))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height bounds) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height bounds) (+ y0 h))
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

(defnk passthrough-textureset
  [textureset]
  textureset)

(n/defnode AtlasRender
  (input  textureset s/Any)

  (output textureset s/Any            :cached passthrough-textureset)
  (output shader s/Any                :cached produce-shader)
  (output vertex-buffer s/Any         :cached produce-renderable-vertex-buffer)
  (output outline-vertex-buffer s/Any :cached produce-outline-vertex-buffer)
  (output gpu-texture s/Any           :cached produce-gpu-texture)
  (output renderable RenderData       produce-renderable))

(def ^:private outline-vertices-per-placement 4)
(def ^:private vertex-size (.getNumber TextureSetProto$Constants/VERTEX_SIZE))

(sm/defn textureset->texcoords
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->uv-only (* 2 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            u0 (* x0 x-scale)
            u1 (* x1 x-scale)
            v0 (* y0 y-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [u0 v0])
          (conj! [u1 v1]))))
    (persistent! vbuf)))

(sm/defn textureset->vertices
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->engine-format-texture (* 6 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            w2 (* (.width coord) 0.5)
            h2 (* (.height coord) 0.5)
            u0 (to-short-uv (* x0 x-scale))
            u1 (to-short-uv (* x1 x-scale))
            v0 (to-short-uv (* y0 y-scale))
            v1 (to-short-uv (* y1 y-scale))]
        (doto vbuf
          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2  (- h2) 0 u1 v1])
          (conj! [   w2     h2  0 u1 v0])

          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2     h2  0 u1 v0])
          (conj! [(- w2)    h2  0 v0 v0]))))
    (persistent! vbuf)))

(sm/defn textureset->outline-vertices
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->engine-format-texture (* 6 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            w2 (* (.width coord) 0.5)
            h2 (* (.height coord) 0.5)
            u0 (to-short-uv (* x0 x-scale))
            u1 (to-short-uv (* x1 x-scale))
            v0 (to-short-uv (* y0 y-scale))
            v1 (to-short-uv (* y1 y-scale))]
        (doto vbuf
          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2  (- h2) 0 u1 v1])
          (conj! [   w2     h2  0 u1 v0])
          (conj! [(- w2)    h2  0 v0 v0]))))
    (persistent! vbuf)))

(defn build-animation
  [anim begin]
  #_(s/validate Animation anim)
  (let [start     (int begin)
        end       (int (+ begin (* 6 (count (:images anim)))))]
    (.build
      (doto (TextureSetProto$TextureSetAnimation/newBuilder)
         (.setId                  (:id anim))
         (.setWidth               (int (:width  (first (:images anim)))))
         (.setHeight              (int (:height (first (:images anim)))))
         (.setStart               start)
         (.setEnd                 end)
         (protobuf/set-if-present :playback anim)
         (protobuf/set-if-present :fps anim)
         (protobuf/set-if-present :flip-horizontal anim)
         (protobuf/set-if-present :flip-vertical anim)
         (protobuf/set-if-present :is-animation anim)))))

(defn build-animations
  [start-idx aseq]
  (let [animations (remove #(empty? (:images %)) aseq)
        starts (into [start-idx] (map #(+ start-idx (* 6 (count (:images %)))) animations))]
    (map (fn [anim start] (build-animation anim start)) animations starts)))

(sm/defn extract-image-coords :- [Rect]
  "Given an map of paths to coordinates and an animation,
   return a seq of coordinates representing the images in that animation."
  [coord-index :- {} animation :- Animation]
  (map #(get coord-index (get-in % [:path :path])) (:images animation)))

(sm/defn index-coords-by-path
  "Given a list of coordinates, produce a map of path -> coordinates."
  [coords :- [Rect]]
  (zipmap (map #(get-in % [:path :path]) coords) coords))

(sm/defn get-animation-image-coords :- [[Rect]]
  "Given a set of Rect coordinates and a list of animations,
   return a list of lists of coordinates for the images
   in the animations."
  [coords :- [Rect] animations :- [Animation]]
  (let [idx (index-coords-by-path coords)]
    (map #(extract-image-coords idx %) animations)))

(defn texturesetc-protocol-buffer
  [texture-name textureset]
  #_(s/validate TextureSet textureset)
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        anims      (remove #(empty? (:images %)) (.animations textureset))
        acoords    (get-animation-image-coords coords anims)
        n-rects    (count coords)
        n-vertices (reduce + n-rects (map #(count (.images %)) anims))]
    (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               texture-name)
            (.setTexCoords             (byte-pack (textureset->texcoords textureset)))
            (.addAllAnimations         (build-animations (* 6 n-rects) anims))

            (.addAllVertexStart        (vertex-starts n-vertices))
            (.addAllVertexCount        (vertex-counts n-vertices))
            (.setVertices              (byte-pack (textureset->vertices textureset)))

            (.addAllOutlineVertexStart (take n-vertices (take-nth 4 integers)))
            (.addAllOutlineVertexCount (take n-vertices (repeat (int 4))))
            (.setOutlineVertices       (byte-pack (textureset->outline-vertices textureset)))

            (.setTileCount             (int 0))))))

(defnk compile-texturesetc :- s/Bool
  [this g project textureset :- TextureSet]
  (file/write-file (:textureset-filename this)
    (.toByteArray (texturesetc-protocol-buffer (:texture-name this) textureset))))

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
  [this g project textureset :- TextureSet]
  (file/write-file (:texture-filename this)
    (.toByteArray (texturec-protocol-buffer (->engine-format (:packed-image textureset)))))
  :ok)

(n/defnode TextureSave
  (input textureset TextureSet)

  (property texture-filename    s/Str (default ""))
  (property texture-name        s/Str)
  (property textureset-filename s/Str (default ""))

  (output   texturec s/Any :on-update compile-texturec)
  (output   texturesetc s/Any :on-update compile-texturesetc))

(defn on-edit
  [project-node editor-site atlas-node]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
        (let [atlas-render (ds/add (n/construct AtlasRender))
              background   (ds/add (n/construct background/Gradient))
              grid         (ds/add (n/construct grid/Grid))
              camera       (ds/add (n/construct CameraController :camera (make-camera :orthographic)))]
          (ds/connect atlas-node   :textureset atlas-render :textureset)
          (ds/connect camera       :camera     grid         :camera)
          (ds/connect camera       :camera     editor       :view-camera)
          (ds/connect camera       :self       editor       :controller)
          (ds/connect background   :renderable editor       :renderables)
          (ds/connect atlas-render :renderable editor       :renderables)
          (ds/connect grid         :renderable editor       :renderables)
          (ds/connect atlas-node   :aabb       editor       :aabb))
        editor)))

(defn- bind-image-connections
  [img-node target-node]
  (when (:image (t/outputs img-node))
    (ds/connect img-node :image target-node :images))
  (when (:tree (t/outputs img-node))
    (ds/connect img-node :tree  target-node :children))  )

(defn- bind-images
  [image-nodes target-node]
  (doseq [img image-nodes]
    (bind-image-connections img target-node)))

(defn construct-ancillary-nodes
  [self locator input]
  (let [atlas (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))]
    (ds/set-property self :margin (:margin atlas))
    (ds/set-property self :extrude-borders (:extrude-borders atlas))
    (doseq [anim (:animations atlas)
            :let [anim-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (select-keys anim [:flip-horizontal :flip-vertical :fps :playback :id]))))]]
      (bind-images (map #(lookup locator (:image %)) (:images anim)) anim-node)
      (ds/connect anim-node :animation self :animations)
      (ds/connect anim-node :tree      self :children))
    (bind-images (map #(lookup locator (:image %)) (:images atlas)) self)
    self))

(defn construct-compiler
  [self]
  (let [path (:filename self)
        compiler (ds/add (n/construct TextureSave
                           :texture-name        (clojure.string/replace (local-path (replace-extension path "texturesetc")) "content/" "")
                           :textureset-filename (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturesetc")) path)
                           :texture-filename    (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturec")) path)))]
    (ds/connect self :textureset compiler :textureset)
    self))

(n/defnode AtlasNode
  (inherits n/OutlineNode)
  (inherits AtlasProperties)
  (inherits AtlasSave)

  (on :load
    (doto self
      (construct-ancillary-nodes (:project event) (:filename self))
      (construct-compiler)
      (ds/set-property :dirty false))))

(defn add-image
  [evt]
  (when-let [target (first (filter #(= AtlasNode (node-type %)) (sel/selected-nodes evt)))]
    (let [project-node  (p/project-enclosing target)
          images-to-add (p/select-resources project-node ["png" "jpg"] "Add Image(s)" true)]
      (ds/transactional
        (doseq [img images-to-add]
          (ds/connect img :image target :images)
          (ds/connect img :tree  target :children))))))

(defcommand add-image-command
  "com.dynamo.cr.menu-items.scene"
  "com.dynamo.cr.builtins.editors.atlas.add-image"
  "Add Image")

(defhandler add-image-handler add-image-command #'add-image)

(when (ds/in-transaction?)
  (p/register-editor "atlas" #'on-edit)
  (p/register-node-type "atlas" AtlasNode))
