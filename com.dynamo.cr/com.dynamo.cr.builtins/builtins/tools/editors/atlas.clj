(ns editors.atlas
  (:require [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [service.log :as log]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.editors :as ed]
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
            [dynamo.selection :as sel]
            [dynamo.system :as ds]
            [internal.ui.scene-editor :as ius]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [com.dynamo.tile.proto Tile$Playback]
            [com.jogamp.opengl.util.awt TextRenderer]
            [dynamo.types Animation Camera Image TextureSet Rect EngineFormatTexture AABB]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [javax.media.opengl.glu GLU]
            [javax.vecmath Matrix4d]
            [org.eclipse.swt SWT]
            [org.eclipse.ui IEditorSite]))

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

(n/defnode AnimationGroupNode
  (inherits n/OutlineNode)

  (input images [Image])

  (property fps             dp/NonNegativeInt (default 30))
  (property flip-horizontal s/Bool)
  (property flip-vertical   s/Bool)
  (property playback        AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (output animation Animation
    (fnk [this id images :- [Image] fps flip-horizontal flip-vertical playback]
      (->Animation id images fps flip-horizontal flip-vertical playback)))

  (property id s/Str))

(defn- consolidate
  [images animations]
  (seq (into #{} (flatten (concat images (map :images animations))))))

(defnk produce-textureset :- TextureSet
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (let [textureset (tex/pack-textures margin extrude-borders (consolidate images animations))]
    (assoc textureset :animations animations)))

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

(defn vertex-starts [n-vertices] (take n-vertices (take-nth 6 integers)))
(defn vertex-counts [n-vertices] (take n-vertices (repeat (int 6))))

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer textureset]
  (let [image ^BufferedImage (.packed-image textureset)]
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

(defn render-textureset
  [ctx gl textureset vertex-binding gpu-texture]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" (texture/texture-unit-index gpu-texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (count (:coords textureset))))))

(defn render-quad
  [ctx gl textureset vertex-binding gpu-texture i]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
   (shader/set-uniform atlas-shader gl "texture" (texture/texture-unit-index gpu-texture))
   (gl/gl-draw-arrays gl GL/GL_TRIANGLES (* 6 i) 6)))

(defn selection-renderables
  [this textureset vertex-binding gpu-texture]
  (let [project-root (p/project-root-node this)]
    (map-indexed (fn [i rect]
                   {:world-transform g/Identity4d
                    :select-name (:_id (t/lookup project-root (:path rect)))
                    :render-fn (fn [ctx gl glu text-renderer] (render-quad ctx gl textureset vertex-binding gpu-texture i))})
                 (:coords textureset))))

(defnk produce-renderable :- RenderData
  [this textureset vertex-binding gpu-texture]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer textureset))}]
   pass/transparent
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-textureset ctx gl textureset vertex-binding gpu-texture))}]
   pass/selection
   (selection-renderables this textureset vertex-binding gpu-texture)})

(defnk produce-renderable-vertex-buffer
  [[:textureset aabb coords]]
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
  [[:textureset aabb coords]]
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
  (input textureset s/Any)

  (output vertex-buffer s/Any         :cached produce-renderable-vertex-buffer)
  (output outline-vertex-buffer s/Any :cached produce-outline-vertex-buffer)
  (output vertex-binding s/Any        :cached (fnk [vertex-buffer] (vtx/use-with vertex-buffer atlas-shader)))
  (output renderable RenderData       produce-renderable))

(def ^:private outline-vertices-per-placement 4)
(def ^:private vertex-size (.getNumber TextureSetProto$Constants/VERTEX_SIZE))

(sm/defn textureset->texcoords
  [{:keys [coords] :as textureset}]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
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
  [{:keys [coords] :as textureset}]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        vbuf       (->engine-format-texture (* 6 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            w2 (* (.width coord) 0.5)
            h2 (* (.height coord) 0.5)
            u0 (g/to-short-uv (* x0 x-scale))
            u1 (g/to-short-uv (* x1 x-scale))
            v0 (g/to-short-uv (* y0 y-scale))
            v1 (g/to-short-uv (* y1 y-scale))]
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
            u0 (g/to-short-uv (* x0 x-scale))
            u1 (g/to-short-uv (* x1 x-scale))
            v0 (g/to-short-uv (* y0 y-scale))
            v1 (g/to-short-uv (* y1 y-scale))]
        (doto vbuf
          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2  (- h2) 0 u1 v1])
          (conj! [   w2     h2  0 u1 v0])
          (conj! [(- w2)    h2  0 v0 v0]))))
    (persistent! vbuf)))

(defn build-animation
  [anim begin]
  (let [start     (int begin)
        end       (int (+ begin (* 6 (count (:images anim)))))]
    (.build
      (doto (TextureSetProto$TextureSetAnimation/newBuilder)
         (.setId                  (:id anim))
         (.setWidth               (int (:width  (first (:images anim)))))
         (.setHeight              (int (:height (first (:images anim)))))
         (.setStart               start)
         (.setEnd                 end)
         (protobuf/set-if-present :playback anim (partial protobuf/val->pb-enum Tile$Playback))
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
  [texture-name {:keys [coords] :as textureset}]
  #_(s/validate TextureSet textureset)
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
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
  [this g project textureset :- TextureSet]
  (file/write-file (:texture-filename this)
    (.toByteArray (texturec-protocol-buffer (tex/->engine-format (:packed-image textureset)))))
  :ok)

(n/defnode TextureSave
  (input textureset TextureSet)

  (property texture-filename    s/Str (default ""))
  (property texture-name        s/Str)
  (property textureset-filename s/Str (default ""))

  (output   texturec    s/Any :on-update compile-texturec)
  (output   texturesetc s/Any :on-update compile-texturesetc))

(defn find-nodes-at-point [this x y]
  (let [factory (gl/glfactory)
        context (.createExternalGLContext factory)
        [renderable-inputs view-camera] (n/get-node-inputs this :renderables :view-camera)
        renderables (apply merge-with concat renderable-inputs)
        pick-rect {:x x :y (- (:bottom (:viewport view-camera)) y) :width 1 :height 1}]
    (ius/selection-renderer context renderables view-camera pick-rect)))

(defn- not-camera-movement?
  "True if the event does not have keyboard modifier-keys for a
  camera movement action (CTRL or ALT). Note that this won't
  necessarily apply to mouse-up events because the modifier keys
  can be released before the mouse button."
  [event]
  (zero? (bit-and (:state-mask event) (bit-or SWT/CTRL SWT/ALT))))

(defn- selection-event?
  [event]
  (and (not-camera-movement? event)
       (= 1 (:button event))))

(defn- deselect-all
  [selection-node]
  (doseq [[node label] (ds/sources-of selection-node :selected-nodes)]
    (ds/disconnect node label selection-node :selected-nodes)))

(defn- select-nodes
  [selection-node nodes]
  (doseq [node nodes]
    (ds/connect node :self selection-node :selected-nodes)))

(defn- toggle-selection
  [selection-node nodes]
  (let [selected-nodes-labels (into {} (ds/sources-of selection-node :selected-nodes))]
    (doseq [node nodes]
      (if-let [label (get selected-nodes-labels node)]
        (ds/disconnect node label selection-node :selected-nodes)
        (ds/connect node :self selection-node :selected-nodes)))))

(defn- selection-mode
  [event]
  (if (zero? (bit-and (:state-mask event) (bit-or SWT/COMMAND SWT/SHIFT)))
    :replace
    :toggle))

(n/defnode SelectionController
  (input renderables [t/RenderData])
  (input view-camera Camera)
  (input selection-node s/Any :inject)
  (on :mouse-down
      (when (selection-event? event)
        (let [{:keys [x y]} event
              {:keys [world-ref]} self
              [selection-node] (n/get-node-inputs self :selection-node)
              nodes (map #(ds/node world-ref %) (find-nodes-at-point self x y))]
          (prn :SelectionController.mouse-down x y)
          (case (selection-mode event)
            :replace (do (deselect-all selection-node)
                         (select-nodes selection-node nodes))
            :toggle (toggle-selection selection-node nodes))))))

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

(defn on-edit
  [project-node ^IEditorSite editor-site atlas-node]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
        (let [atlas-render (ds/add (n/construct AtlasRender))
              background   (ds/add (n/construct background/Gradient))
              grid         (ds/add (n/construct grid/Grid))
              camera       (ds/add (n/construct CameraController :camera (make-camera :orthographic)))
              controller   (ds/add (n/construct BroadcastController))
              selector     (ds/add (n/construct SelectionController))]
          (ds/connect atlas-node   :textureset  atlas-render :textureset)
          (ds/connect atlas-node   :gpu-texture atlas-render :gpu-texture)
          (ds/connect camera       :camera      grid         :camera)
          (ds/connect camera       :camera      editor       :view-camera)
          (ds/connect camera       :self        controller   :controllers)
          (ds/connect selector     :self        controller   :controllers)
          (ds/connect atlas-render :renderable  selector     :renderables)
          (ds/connect camera       :camera      selector     :view-camera)
          (ds/connect controller   :self        editor       :controller)
          (ds/connect background   :renderable  editor       :renderables)
          (ds/connect atlas-render :renderable  editor       :renderables)
          (ds/connect grid         :renderable  editor       :renderables)
          (ds/connect atlas-node   :aabb        editor       :aabb))
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

(defn remove-ancillary-nodes
  [self]
  (doseq [[animation-group _] (ds/sources-of self :animations)]
    (ds/delete animation-group))
  (doseq [[image _] (ds/sources-of self :images)]
    (ds/disconnect image :image self :images)
    (ds/disconnect image :tree  self :children)))

(defn construct-compiler
  [self]
  (let [path (:filename self)
        compiler (ds/add (n/construct TextureSave
                           :texture-name        (clojure.string/replace (local-path (replace-extension path "texturesetc")) "content/" "")
                           :textureset-filename (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturesetc")) path)
                           :texture-filename    (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturec")) path)))]
    (ds/connect self :textureset compiler :textureset)
    self))

(defn remove-compiler
  [self]
  (let [candidates (ds/nodes-consuming self :textureset)]
    (doseq [[compiler _]  (filter #(= TextureSave (t/node-type (first %))) candidates)]
      (ds/delete compiler))))

(n/defnode AtlasNode
  "This node represents an actual Atlas. It accepts a collection
   of images and animations. It emits a packed textureset.

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
   textureset `[dynamo.types/TextureSet]` - A data structure with full access to the original image bounds, their coordinates in the packed image, the BufferedImage, and outline coordinates.\"
   "
  (inherits n/OutlineNode)
  (inherits n/ResourceNode)
  (inherits n/Saveable)

  (input images [Image])
  (input animations [Animation])

  (property margin          dp/NonNegativeInt (default 0))
  (property extrude-borders dp/NonNegativeInt (default 0))
  (property filename (s/protocol PathManipulation) (visible false))

  (output aabb        AABB               (fnk [textureset] (g/rect->aabb (:aabb textureset))))
  (output gpu-texture s/Any      :cached (fnk [textureset] (texture/image-texture (:packed-image textureset))))
  (output save        s/Keyword          save-atlas-file)
  (output text-format s/Str              get-text-format)
  (output textureset  TextureSet :cached :substitute-value (tex/blank-textureset) produce-textureset)

  (on :load
    (doto self
      (construct-ancillary-nodes (:project event) (:filename self))
      (construct-compiler)
      (ds/set-property :dirty false)))

  (on :unload
    (doto self
      (remove-ancillary-nodes)
      (remove-compiler))))

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
