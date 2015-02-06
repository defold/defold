(ns editors.atlas
  (:require [clojure.set :refer [difference union]]
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
            [internal.render.pass :as pass]
            [internal.repaint :as repaint])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [com.dynamo.tile.proto Tile$Playback]
            [com.jogamp.opengl.util.awt TextRenderer]
            [dynamo.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [javax.media.opengl.glu GLU]
            [javax.vecmath Matrix4d]
            [org.eclipse.swt SWT]
            [org.eclipse.ui IEditorSite]))

(def integers (iterate (comp int inc) (int 0)))

(def min-drag-move
  "Minimum number of pixels the mouse must travel to initiate a click-and-drag."
  2)

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
  (inherits n/AutowireResources)

  (property images dp/ImageResourceList)

  (property fps             dp/NonNegativeInt (default 30))
  (property flip-horizontal s/Bool)
  (property flip-vertical   s/Bool)
  (property playback        AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (output animation Animation
    (fnk [this id images :- [Image] fps flip-horizontal flip-vertical playback]
      (->Animation id images fps flip-horizontal flip-vertical playback)))

  (property id s/Str))

(defn- consolidate
  [animations]
  (seq (into #{} (mapcat :images animations))))

(defnk produce-texture-packing :- TexturePacking
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (let [animations (concat animations (map tex/animation-from-image images))
        _          (assert (pos? (count animations)))
        images     (consolidate animations)
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
    (shader/set-uniform atlas-shader gl "texture" (texture/texture-unit-index gl gpu-texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (count (:coords texture-packing))))))

(defn render-quad
  [ctx gl texture-packing vertex-binding gpu-texture i]
  (gl/with-enabled gl [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" (texture/texture-unit-index gl gpu-texture))
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES (* 6 i) 6)))

(defn selection-renderables
  [this texture-packing vertex-binding gpu-texture]
  (let [project-root (p/project-root-node this)]
    (map-indexed (fn [i rect]
                   {:world-transform g/Identity4d
                    :select-name (:_id (t/lookup project-root (:path rect)))
                    :render-fn (fn [ctx gl glu text-renderer] (render-quad ctx gl texture-packing vertex-binding gpu-texture i))})
                 (:coords texture-packing))))

(defn render-selection-outline
  [ctx ^GL2 gl this texture-packing rect]
  (let [bounds (:aabb texture-packing)
        {:keys [x y width height]} rect
        left x
        right (+ x width)
        bottom (- (:height bounds) y)
        top (- (:height bounds) (+ y height))]
    (.glColor3ub gl 75 -1 -117)  ; #4bff8b bright green
    (.glBegin gl GL2/GL_LINE_LOOP)
    (.glVertex2i gl left top)
    (.glVertex2i gl right top)
    (.glVertex2i gl right bottom)
    (.glVertex2i gl left bottom)
    (.glEnd gl)))

(defn selection-outline-renderables
  [this texture-packing selection]
  (let [project-root (p/project-root-node this)
        current-selection (set @selection)]
    (vec
      (keep
        (fn [rect]
          (let [[pending-selection] (n/get-node-inputs this :pending-selection)
                node-id (:_id (t/lookup project-root (:path rect)))
                active-selection (or @pending-selection current-selection)]
            (when (contains? active-selection node-id)
              {:world-transform g/Identity4d
               :render-fn (fn [ctx gl glu text-renderer]
                            (render-selection-outline ctx gl this texture-packing rect))})))
        (:coords texture-packing)))))

(defnk produce-renderable :- RenderData
  [this texture-packing selection vertex-binding gpu-texture]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer texture-packing))}]
   pass/transparent
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-texture-packing ctx gl texture-packing vertex-binding gpu-texture))}]
   pass/outline
   (selection-outline-renderables this texture-packing selection)
   pass/selection
   (selection-renderables this texture-packing vertex-binding gpu-texture)})

(defn- render-selection-box
  [ctx ^GL2 gl glu selection-box]
  (let [{:keys [left right bottom top]} selection-box]
    (.glColor3ub gl 115 -81 -52)
    (.glBegin gl GL2/GL_LINE_LOOP)
    (.glVertex2i gl left top)
    (.glVertex2i gl right top)
    (.glVertex2i gl right bottom)
    (.glVertex2i gl left bottom)
    (.glEnd gl)
    (.glBegin gl GL2/GL_QUADS)
    (.glColor4ub gl 115 -81 -52 64)
    (.glVertex2i gl left top)
    (.glVertex2i gl right top)
    (.glVertex2i gl right bottom)
    (.glVertex2i gl left bottom)
    (.glEnd gl)))

(defnk selection-box-renderable
  [ui-state]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn (fn [ctx gl glu text-renderer]
                  (let [{:keys [selection-region]} @ui-state]
                    (when selection-region
                      (render-selection-box ctx gl glu selection-region))))}]})

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
  (input selection s/Any :inject)
  (input pending-selection s/Any :inject)

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
  #_(s/validate TextureSet textureset)
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

(defnk selection-region
  "Returns the Region currently being selected by a mouse drag."
  [start-x start-y current-x current-y]
  (t/map->Region
    {:left (min start-x current-x)
     :right (max start-x current-x)
     :top (min start-y current-y)
     :bottom (max start-y current-y)}))

(def min-selection-size
  "Minimum size, in pixels, of the selection area around a click.
  Analagous to com.dynamo.cr.sceneed.ui.RenderView.MIN_SELECTION_BOX,
  which is 16 pixels."
  1)

(defn pick-rect
  "Given a Region, returns a Rect with x & y at the *center* of the region."
  [pick-region]
  (let [{:keys [left right top bottom]} pick-region
        width (- right left)
        height (- bottom top)
        x (+ left (quot width 2))
        y (+ top (quot height 2))]
    (t/rect x y width height)))

(defn rect-in-viewport
  "Corrects the Rect for inverted screen coordinates based on the viewport Region."
  [rect viewport]
  (assoc rect :y (- (:bottom viewport) (:y rect))))

(defn min-selection-rect
  [rect]
  (assoc rect
    :width (max min-selection-size (:width rect))
    :height (max min-selection-size (:height rect))))

(defn find-nodes-in-selection [ui-state]
  (let [{:keys [glcontext renderable-inputs view-camera]} ui-state
        renderables (apply merge-with concat renderable-inputs)
        {:keys [viewport]} view-camera
        pick-rect (-> ui-state
                    selection-region
                    pick-rect
                    min-selection-rect
                    (rect-in-viewport viewport))]
    (ius/selection-renderer glcontext renderables view-camera pick-rect)))

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

(defn- selection-mode
  "Either :replace for normal selection mode or :toggle for multi-select.
  On Mac: COMMAND or SHIFT.
  On non-Mac: CTRL or SHIFT."
  [event]
  ;; SWT/MOD1 maps to COMMAND on Mac and CTRL elsewhere
  (if (zero? (bit-and (:state-mask event) (bit-or SWT/MOD1 SWT/SHIFT)))
    :replace
    :toggle))

(defn- selected-node-ids
  [selection-node]
  (set (map (comp :_id first) (ds/sources-of selection-node :selected-nodes))))

(defn- toggle
  "Returns a new set by toggling the elements in the 'clicked' set
  between present/absent in the 'previous' set."
  [previous clicked]
   (union
     (difference previous clicked)
     (difference clicked previous)))

(defn- drag-move?
  "True if the mouse moved far enough for this to be considered a click-and-drag."
  [self event]
  (let [{:keys [start-x start-y]} self
        {:keys [x y]} event]
    (or (< min-drag-move (Math/abs (- x start-x)))
        (< min-drag-move (Math/abs (- y start-y))))))

(defn pending-selection
  "Returns a set of node IDs for the selection being created by the
  current click or click-and-drag operation."
  [ui-state event]
  (let [{:keys [start-x start-y dragging previous-selection]} ui-state
        clicked (set (cond->> (find-nodes-in-selection ui-state)
                       (not dragging) (take 1)))]
    (case (selection-mode event)
      :replace clicked
      :toggle (toggle previous-selection clicked))))

(defn complete-selection
  [self event]
  (let [ui-state @(:ui-state self)
        {:keys [world-ref]} self
        {:keys [default-selection selection-node]} ui-state
        new-node-ids (pending-selection ui-state event)
        nodes (or (seq (map #(ds/node world-ref %) new-node-ids))
                [default-selection])]
    (deselect-all selection-node)
    (select-nodes selection-node nodes)))

(defn update-drag-state [ui-state event]
  (as-> ui-state state
    (assoc state
      :dragging true
      :current-x (:x event)
      :current-y (:y event))
    (assoc state :selection-region (selection-region state))
    (assoc state :pending-selection (pending-selection state event))))

(n/defnode SelectionController
  (property ui-state s/Any (default (constantly (atom {}))))
  (property pending-selection s/Any (default (constantly (atom nil))))

  (input glcontext GLContext :inject)
  (input renderables [t/RenderData])
  (input view-camera Camera)
  (input selection-node s/Any :inject)
  (input default-selection s/Any)

  (output renderable t/RenderData selection-box-renderable)
  (output pending-selection s/Any (fnk [pending-selection] pending-selection))

  (on :mouse-down
    (when (selection-event? event)
      (let [[selection-node default-selection glcontext renderables view-camera]
              (n/get-node-inputs self :selection-node :default-selection :glcontext :renderables :view-camera)
            editor-node (ds/node-consuming self :renderable)
            previous-selection (disj (selected-node-ids selection-node)
                                 (:_id default-selection))]
        (reset! (:pending-selection self) #{})
        (swap! (:ui-state self) assoc
          :selecting true
          :selection-node selection-node
          :editor-node editor-node
          :default-selection default-selection
          :glcontext glcontext
          :renderable-inputs renderables
          :view-camera view-camera
          :previous-selection previous-selection
          :start-x (:x event)
          :start-y (:y event)
          :current-x (:x event)
          :current-y (:y event)))))

  (on :mouse-move
    (let [{:keys [selecting dragging editor-node] :as ui-state} @(:ui-state self)]
      (when (and selecting (or dragging (drag-move? ui-state event)))
        (let [new-ui-state (update-drag-state ui-state event)]
          ;; Don't want rendering inside swap!, and this is all on the
          ;; event-handling thread so reset! is safe.
          (reset! (:ui-state self) new-ui-state)
          (reset! (:pending-selection self) (:pending-selection new-ui-state)))
        (repaint/schedule-repaint (-> self :world-ref deref :repaint-needed) [editor-node]))))

  (on :mouse-up
    (let [{:keys [selecting] :as ui-state} @(:ui-state self)]
      (when selecting
        (complete-selection self event)
        (reset! (:ui-state self) {})
        (reset! (:pending-selection self) nil)))))

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
          (ds/connect atlas-node   :texture-packing atlas-render :texture-packing)
          (ds/connect atlas-node   :gpu-texture     atlas-render :gpu-texture)
          (ds/connect atlas-node   :self            selector     :default-selection)
          (ds/connect camera       :camera          grid         :camera)
          (ds/connect camera       :camera          editor       :view-camera)
          (ds/connect camera       :self            controller   :controllers)
          (ds/connect selector     :self            controller   :controllers)
          (ds/connect atlas-render :renderable      selector     :renderables)
          (ds/connect camera       :camera          selector     :view-camera)
          (ds/connect controller   :self            editor       :controller)
          (ds/connect background   :renderable      editor       :renderables)
          (ds/connect atlas-render :renderable      editor       :renderables)
          (ds/connect grid         :renderable      editor       :renderables)
          (ds/connect selector     :renderable      editor       :renderables)
          (ds/connect atlas-node   :aabb            editor       :aabb))
        editor)))

(defn construct-ancillary-nodes
  [self input]
  (let [atlas (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))]
    (ds/set-property self :margin (:margin atlas))
    (ds/set-property self :extrude-borders (:extrude-borders atlas))
    (doseq [anim (:animations atlas)
            :let [anim-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (select-keys anim [:flip-horizontal :flip-vertical :fps :playback :id]))))]]
      (ds/set-property anim-node :images (mapv :image (:images anim)))
      (ds/connect anim-node :animation self :animations)
      (ds/connect anim-node :tree      self :children))
    (ds/set-property self :images (mapv :image (:images atlas)))
    self))

(defn remove-ancillary-nodes
  [self]
  (doseq [[animation-group _] (ds/sources-of self :animations)]
    (ds/delete animation-group)))

(defn construct-compiler
  [self]
  (let [path (:filename self)
        compiler (ds/add (n/construct TextureSave
                           :texture-name        (clojure.string/replace (local-path (replace-extension path "texturesetc")) "content/" "")
                           :textureset-filename (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturesetc")) path)
                           :texture-filename    (if (satisfies? file/ProjectRelative path) (file/in-build-directory (replace-extension path "texturec")) path)))]
    (ds/connect self :textureset   compiler :textureset)
    (ds/connect self :packed-image compiler :packed-image)
    self))

(defn remove-compiler
  [self]
  (let [candidates (ds/nodes-consuming self :textureset)]
    (doseq [[compiler _]  (filter #(= TextureSave (t/node-type (first %))) candidates)]
      (ds/delete compiler))))

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
  (inherits n/AutowireResources)
  (inherits n/ResourceNode)
  (inherits n/Saveable)

  (input animations [Animation])

  (property images dp/ImageResourceList (visible false))

  (property margin          dp/NonNegativeInt (default 0))
  (property extrude-borders dp/NonNegativeInt (default 0))
  (property filename (s/protocol PathManipulation) (visible false))

  (output aabb            AABB               (fnk [texture-packing] (g/rect->aabb (:aabb texture-packing))))
  (output gpu-texture     s/Any      :cached (fnk [packed-image] (texture/image-texture packed-image)))
  (output save            s/Keyword          save-atlas-file)
  (output text-format     s/Str              get-text-format)
  (output texture-packing TexturePacking :cached :substitute-value (tex/blank-texture-packing) produce-texture-packing)
  (output packed-image    BufferedImage  :cached (fnk [texture-packing] (:packed-image texture-packing)))
  (output textureset      TextureSet     :cached produce-textureset)

  (on :load
    (doto self
      (construct-ancillary-nodes (:filename self))
      (construct-compiler)
      #_(ds/set-property :dirty false)))

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
