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
            [java.nio IntBuffer]
            [javax.vecmath Matrix4d]
            [org.eclipse.swt SWT]
            [org.eclipse.ui IEditorSite]))

(def min-drag-move
  "Minimum number of pixels the mouse must travel to initiate a click-and-drag."
  2)

(def toggle-select-modifiers
  "Bitmask of modifier keys which turn on toggle selection.
  COMMAND or SHIFT on Mac OS X.
  CTRL or SHIFT everywhere else."
  (bit-or SWT/MOD1 SWT/SHIFT))

(vtx/defvertex engine-format-texture
  (vec3.float position)
  (vec2.short texcoord0 true))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(vtx/defvertex uv-only
  (vec2 uv))

(declare tex-outline-vertices)

(defn- input-connections
  [graph node input-label]
  (mapv (fn [[source-node output-label]] [(:_id source-node) output-label])
    (ds/sources-of graph node input-label)))

(defn connect-outline-children
  [input-labels input-name transaction graph self label kind inputs-touched]
  (when (some (set input-labels) inputs-touched)
    (let [children-before (input-connections (ds/in-transaction-graph transaction) self input-name)
          children-after  (mapcat #(input-connections graph self %) input-labels)]
      (doseq [[n l] children-before]
        (ds/disconnect {:_id n} l self input-name))
      (doseq [[n _] children-after]
        (ds/connect {:_id n} :outline-tree self input-name)))))

(defn add-images-command
  [{:keys [node-ref input-label] :as context}]
  (when-let [target @node-ref]
    (let [project-node  (p/project-enclosing target)
          images-to-add (p/select-resources project-node ["png" "jpg"] "Add Image(s)" true)]
      (ds/transactional
        (ds/tx-label "Add Images")
        (doseq [img images-to-add]
          (ds/connect img :content target input-label))))))

(defn attach-commands-to-outline-item
  [commands outline-item]
  (update-in outline-item [:commands] concat commands))

(declare copy-child-command)
(declare cut-child-command)
(declare paste-child-command)
(declare disconnect-to-delete-command)

(defn clipboard-commands [clipboard-ref child-node-ref child-output-label parent-node-ref parent-input-label]
  (let [context {:clipboard-ref   clipboard-ref
                 :source-node-ref child-node-ref
                 :source-label    child-output-label
                 :target-node-ref parent-node-ref
                 :target-label    parent-input-label}]
    [{:label "Copy"   :enabled true :command-fn copy-child-command           :context context}
     {:label "Cut"    :enabled true :command-fn cut-child-command            :context context}
     {:label "Paste"  :enabled true :command-fn paste-child-command          :context context}
     {:label "Delete" :enabled true :command-fn disconnect-to-delete-command :context context}]))

(defnk animation-group-outline-commands [this]
  [{:label "Add Images" :enabled true :command-fn add-images-command :context {:node-ref (node-ref this) :input-label :images}}])

(defnk animation-group-outline-children [this clipboard images-outline-children]
  (map (fn [outline-item]
         (attach-commands-to-outline-item
           (clipboard-commands clipboard (:node-ref outline-item) :content (t/node-ref this) :images)
           outline-item))
    images-outline-children))

(n/defnode AnimationGroupNode
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [id] id))
  (output outline-commands [t/OutlineCommand] animation-group-outline-commands)

  (input clipboard s/Any :inject)

  (inherits n/AutowireResources)

  (property images dp/ImageResourceList)
  (input images-outline-children [OutlineItem])
  (trigger connect-images-outline-children :input-connections (partial connect-outline-children [:images] :images-outline-children))
  (output outline-children [OutlineItem] animation-group-outline-children)

  (property fps             dp/NonNegativeInt (default 30))
  (property flip-horizontal s/Bool)
  (property flip-vertical   s/Bool)
  (property playback        AnimationPlayback (default :PLAYBACK_ONCE_FORWARD))

  (output animation Animation
    (fnk [this id images :- [Image] fps flip-horizontal flip-vertical playback]
      (->Animation id images fps flip-horizontal flip-vertical playback)))

  (property id s/Str))

(defn- distinct-by [f coll]
  (->> coll
    (reduce (fn [m v] (assoc m (f v) v)) {})
    vals))

(defn- consolidate
  [animations]
  (->> animations
    (mapcat :images)
    (distinct-by :path)))

(defnk produce-texture-packing :- TexturePacking
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (let [animations (concat animations (map tex/animation-from-image images))
        _          (assert (pos? (count animations)))
        images     (consolidate animations)
        _          (assert (pos? (count images)))
        texture-packing (tex/pack-textures margin extrude-borders images)]
    (assoc texture-packing :animations animations)))

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
        ; TODO: may fail due to #87253110 "Atlas texture should not contain multiple instances of the same image"
        ;_ (assert (= 1 (count coords)))
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
        outline-vertices [[x0 y0 0 (g/to-short-uv u0) (g/to-short-uv v1)]
                          [x1 y0 0 (g/to-short-uv u1) (g/to-short-uv v1)]
                          [x1 y1 0 (g/to-short-uv u1) (g/to-short-uv v0)]
                          [x0 y1 0 (g/to-short-uv u0) (g/to-short-uv v0)]]]
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

(defnk produce-textureset :- TextureSet
  [this texture-packing :- TexturePacking]
  (let [animations             (sort-by :id (:animations texture-packing))
        animations             (remove #(empty? (:images %)) animations)
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
  [this filename ^String text-format]
  (file/write-file filename (.getBytes text-format))
  :ok)

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer ^TexturePacking texture-packing]
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
  [this texture-packing selection pending-selection]
  (let [project-root (p/project-root-node this)
        current-selection (set @selection)]
    (vec
      (keep
        (fn [rect]
          (let [node-id (:_id (t/lookup project-root (:path rect)))
                active-selection (or @pending-selection current-selection)]
            (when (contains? active-selection node-id)
              {:world-transform g/Identity4d
               :render-fn (fn [ctx gl glu text-renderer]
                            (render-selection-outline ctx gl this texture-packing rect))})))
        (:coords texture-packing)))))

(defnk produce-renderable :- RenderData
  [this texture-packing selection pending-selection vertex-binding gpu-texture]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer texture-packing))}]
   pass/transparent
   [{:world-transform g/Identity4d
     :render-fn       (fn [ctx gl glu text-renderer] (render-texture-packing ctx gl texture-packing vertex-binding gpu-texture))}]
   pass/outline
   (selection-outline-renderables this texture-packing selection pending-selection)
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
  [[:texture-packing ^Rect aabb coords]]
  (let [vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width aabb))
        y-scale    (/ 1.0 (.height aabb))]
    (doseq [^Rect coord coords]
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
  [[:texture-packing ^Rect aabb coords]]
  (let [vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width aabb))
        y-scale    (/ 1.0 (.height aabb))]
    (doseq [^Rect coord coords]
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

(defn ^TextureSetProto$TextureSet texturesetc-protocol-buffer
  [texture-name textureset]
  #_(s/validate TextureSet textureset)
  (let [animations (sort-by :id (vals (:animations textureset)))
        frames     (mapcat :frames animations)]
    (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               texture-name)
            (.setTexCoords             (byte-pack (:tex-coords textureset)))
            (.addAllAnimations         (build-animations 0 animations))

            (.addAllVertexStart        (map :vertex-start frames))
            (.addAllVertexCount        (map :vertex-count frames))
            (.setVertices              (byte-pack (:vertices textureset)))

            (.addAllAtlasVertexStart   (map :vertex-start frames))
            (.addAllAtlasVertexCount   (map :vertex-count frames))
            (.setAtlasVertices         (byte-pack (:vertices textureset)))

            (.addAllOutlineVertexStart (map :outline-vertex-start frames))
            (.addAllOutlineVertexCount (map :outline-vertex-count frames))
            (.setOutlineVertices       (byte-pack (:outline-vertices textureset)))

            (.setTileCount             (int 0))))))

(defnk compile-texturesetc :- s/Bool
  [this g project textureset :- TextureSet]
  (file/write-file (:textureset-filename this)
    (.toByteArray (texturesetc-protocol-buffer (:texture-name this) textureset)))
  :ok)

(defn- ^Graphics$TextureImage texturec-protocol-buffer
  [^EngineFormatTexture engine-format]
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

(defn make-pick-buffer [] (as-int-buffer (native-order (new-byte-buffer 4 4096))))

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
  (let [{:keys [glcontext renderable-inputs view-camera pick-buffer]} ui-state
        renderables (apply merge-with concat renderable-inputs)
        {:keys [viewport]} view-camera
        pick-rect (-> ui-state
                    selection-region
                    pick-rect
                    min-selection-rect
                    (rect-in-viewport viewport))]
    (ius/selection-renderer glcontext pick-buffer renderables view-camera pick-rect)))

(defn- selection-event?
  "True if the event is the beginning of a mouse-selection. That means:
  1. No other mouse button is already held down;
  2. Neither CTRL nor ALT is held down (camera movement);
  3. The mouse button clicked was button 1."
  [event]
  (and (zero? (bit-and (:state-mask event) (bit-or SWT/BUTTON_MASK SWT/CTRL SWT/ALT)))
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
  "Either :replace for normal selection mode or :toggle for multi-select."
  [modifiers]
  (if (zero? (bit-and modifiers toggle-select-modifiers))
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
    (or (< min-drag-move (Math/abs (long (- x start-x))))
        (< min-drag-move (Math/abs (long (- y start-y)))))))

(defn pending-selection
  "Returns a set of node IDs for the selection being created by the
  current click or click-and-drag operation."
  [ui-state]
  (prn :pending-selection (select-keys ui-state [:start-x  :start-y :current-x :current-y :dragging :previous-selection :modifiers]) (-> ui-state :view-camera :viewport))
  (let [{:keys [start-x start-y dragging previous-selection modifiers]} ui-state
        clicked (set (cond->> (find-nodes-in-selection ui-state)
                       (not dragging) (take 1)))]
    (prn :pending-selection :clicked clicked)
    (case (selection-mode modifiers)
      :replace clicked
      :toggle (toggle previous-selection clicked))))

(defn complete-selection
  [self]
  (let [ui-state @(:ui-state self)
        {:keys [world-ref]} self
        {:keys [default-selection selection-node]} ui-state
        new-node-ids (pending-selection ui-state)
        nodes (or (seq (map #(ds/node world-ref %) new-node-ids))
                [default-selection])]
    (deselect-all selection-node)
    (select-nodes selection-node nodes)))

(defn update-drag-mouse
  "Returns updated UI state after a mouse-move event during a
  click-and-drag selection."
  [ui-state event]
  (as-> ui-state state
    (assoc state
      :dragging true
      :current-x (:x event)
      :current-y (:y event))
    (assoc state :selection-region (selection-region state))))

(defn update-select-keydown
  "Returns updated UI state if the key-down event changed the state
  of the selection modifier keys."
  [ui-state event]
  (if (zero? (bit-and toggle-select-modifiers (:key-code event)))
    ui-state
    (update-in ui-state [:modifiers] bit-or (:key-code event))))

(defn update-select-keyup
  "Returns updated UI state if the key-up event changed the state
  of the selection modifier keys."
  [ui-state event]
  (if (zero? (bit-and toggle-select-modifiers (:key-code event)))
    ui-state
    (update-in ui-state [:modifiers] bit-and-not (:key-code event))))

(n/defnode SelectionController
  (property pick-buffer bytes)
  (property ui-state s/Any)
  (property pending-selection s/Any)

  (input glcontext GLContext :inject)
  (input renderables [pass/RenderData])
  (input view-camera Camera)
  (input selection-node s/Any :inject)
  (input default-selection s/Any)

  (output renderable pass/RenderData selection-box-renderable)
  (output pending-selection s/Any (fnk [pending-selection] pending-selection))

  (on :mouse-down
    (when (selection-event? event)
      (let [[selection-node default-selection glcontext renderables view-camera]
              (n/get-node-inputs self :selection-node :default-selection :glcontext :renderables :view-camera)
            editor-node (ds/node-consuming self :renderable)
            previous-selection (disj (selected-node-ids selection-node)
                                 (:_id default-selection))
            new-ui-state (swap! (:ui-state self) assoc
                           :pick-buffer (.clear ^IntBuffer (:pick-buffer self))
                           :selecting true
                           :selection-node selection-node
                           :modifiers (:state-mask event)
                           :editor-node editor-node
                           :default-selection default-selection
                           :glcontext glcontext
                           :renderable-inputs renderables
                           :view-camera view-camera
                           :previous-selection previous-selection
                           :start-x (:x event)
                           :start-y (:y event)
                           :current-x (:x event)
                           :current-y (:y event))]
        (let [sel (pending-selection new-ui-state)]
          (prn :mouse-down :pending-selection sel)
          (reset! (:pending-selection self) sel))
        (repaint/schedule-repaint (-> self :world-ref deref :repaint-needed) [editor-node]))))

  (on :mouse-move
    (let [{:keys [selecting dragging editor-node] :as ui-state} @(:ui-state self)]
      (when (and selecting (or dragging (drag-move? ui-state event)))
        (let [new-ui-state (update-drag-mouse ui-state event)]
          ;; Don't want rendering inside swap!, and this is all on the
          ;; event-handling thread so reset! is safe.
          (reset! (:ui-state self) new-ui-state)
          (reset! (:pending-selection self) (pending-selection new-ui-state)))
        (repaint/schedule-repaint (-> self :world-ref deref :repaint-needed) [editor-node]))))

  (on :mouse-up
    (when (:selecting @(:ui-state self))
      (complete-selection self)
      (reset! (:ui-state self) {})
      (reset! (:pending-selection self) nil)))

  (on :key-down
    (let [{:keys [selecting editor-node]} @(:ui-state self)]
      (when selecting
        (let [new-ui-state (swap! (:ui-state self) update-select-keydown event)]
          (reset! (:pending-selection self) (pending-selection new-ui-state))
          (repaint/schedule-repaint (-> self :world-ref deref :repaint-needed) [editor-node])))))

  (on :key-up
    (let [{:keys [selecting editor-node]} @(:ui-state self)]
      (when selecting
        (let [new-ui-state (swap! (:ui-state self) update-select-keyup event)]
          (reset! (:pending-selection self) (pending-selection new-ui-state))
          (repaint/schedule-repaint (-> self :world-ref deref :repaint-needed) [editor-node]))))))

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
              selector     (ds/add (n/construct SelectionController :pick-buffer (make-pick-buffer) :pending-selection (atom nil) :ui-state (atom {})))]
          (ds/update-property camera :movements-enabled disj :tumble)
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
          (ds/connect atlas-node   :aabb            editor       :aabb)
          (ds/connect atlas-node   :outline-tree    editor       :outline-tree))
        editor)))

(defn construct-ancillary-nodes
  [self input]
  (let [atlas (protobuf/pb->map (protobuf/read-text AtlasProto$Atlas input))]
    (ds/set-property self :margin (:margin atlas))
    (ds/set-property self :extrude-borders (:extrude-borders atlas))
    (doseq [anim (:animations atlas)
            :let [anim-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (select-keys anim [:flip-horizontal :flip-vertical :fps :playback :id]))))]]
      (ds/set-property anim-node :images (mapv :image (:images anim)))
      (ds/connect anim-node :animation self :animations))
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

(defnk build-atlas-outline-children
  [this clipboard images-outline-children animations-outline-children]
  (concat
    (map (fn [outline-item]
           (attach-commands-to-outline-item
             (clipboard-commands clipboard (:node-ref outline-item) :content (t/node-ref this) :images)
             outline-item))
      images-outline-children)
    (map (fn [outline-item]
           (attach-commands-to-outline-item
             (clipboard-commands clipboard (:node-ref outline-item) :animation (t/node-ref this) :animations)
             outline-item))
      animations-outline-children)))

(declare add-child-node-command)

(defnk atlas-outline-commands [this]
  (concat
    [{:label "Add Images"
      :enabled true
      :command-fn add-images-command
      :context {:node-ref (node-ref this)
                :input-label :images}}
     {:label "Add Animation Group"
      :enabled true
      :command-fn add-child-node-command
      :context {:node-ref (node-ref this)
                :input-label :animations
                :node-type AnimationGroupNode
                :name-property :id
                :base-name "anim"
                :output-label :animation}}]))

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
  (output outline-label s/Str (fnk [] "Atlas"))
  (output outline-commands [t/OutlineCommand] atlas-outline-commands)
  (inherits n/AutowireResources)
  (inherits n/ResourceNode)
  (inherits n/Saveable)

  (input clipboard s/Any :inject)

  (input animations [Animation])
  (input animations-outline-children [OutlineItem])
  (trigger connect-animations-outline-children :input-connections (partial connect-outline-children [:animations] :animations-outline-children))

  (property images dp/ImageResourceList (visible false))
  (input images-outline-children [OutlineItem])
  (trigger connect-images-outline-children :input-connections (partial connect-outline-children [:images] :images-outline-children))

  (output outline-children [OutlineItem] build-atlas-outline-children)

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
      (ds/set-property :dirty false)))

  (on :unload
    (doto self
      (remove-ancillary-nodes)
      (remove-compiler))))

(defn existing-animation-group-names [atlas-node]
  (map (comp :id first) (ds/sources-of atlas-node :animations)))

(defn unique-name [orig-name existing-names]
  (let [[_ base-name num] (re-matches #"^(.*?)([0-9]*)$" orig-name)
        num (Long. (str "0" num))]
    (->> (range)
      (drop (inc num))
      (map (partial str base-name))
      (cons orig-name)
      (remove (set existing-names))
      first)))

(defn add-child-node-command
  [{:keys [node-ref input-label node-type name-property base-name output-label] :as context}]
  (when-let [target @node-ref]
    (let [name (->> (ds/sources-of target input-label)
                    (filter #(= output-label (second %)))
                    (map (comp name-property first))
                    (unique-name base-name))
          project-node (p/project-enclosing target)]
      (ds/transactional
        (ds/in project-node
          (let [child-node (ds/add (n/construct node-type name-property name))]
            (ds/connect child-node output-label target input-label)))))))

(defn duplicate-animation-group-node [atlas-node animation-group-node]
  (ds/transactional
    (ds/in (p/project-enclosing atlas-node)
      (let [name (unique-name (:id animation-group-node) (existing-animation-group-names atlas-node))
            attrs (reduce (fn [m prop] (assoc m prop (get animation-group-node prop))) {} (keys (t/properties animation-group-node)))
            new-node (ds/add (apply n/construct AnimationGroupNode (mapcat identity (assoc attrs :id name))))]
        ;; NOTE: Assumes shallow copy of all input connections.
        (doseq [input (t/inputs animation-group-node)
                [node output] (ds/sources-of animation-group-node input)]
          (ds/connect node output new-node input))
        new-node))))

(defn copy-child-command
  [{:keys [clipboard-ref source-node-ref source-label] :as context}]
  (dosync (ref-set clipboard-ref {:node-ref source-node-ref :node-label source-label})))

(defn cut-child-command
  [{:keys [source-node-ref source-label target-node-ref target-label] :as context}]
  (copy-child-command context)
  (disconnect-to-delete-command context))

(defn paste-child-command
  [{:keys [clipboard-ref target-node-ref] :as context}]
  (when-let [{source-node-ref :node-ref source-label :node-label} @clipboard-ref]
    (ds/transactional
      (condp = [(t/node-type @source-node-ref) (t/node-type @target-node-ref)]
        [AnimationGroupNode AtlasNode]
        (let [n (duplicate-animation-group-node @target-node-ref @source-node-ref)]
          (ds/connect n source-label @target-node-ref :animations))

        [editors.image-node/ImageResourceNode AtlasNode]
        (ds/connect @source-node-ref source-label @target-node-ref :images)

        [editors.image-node/ImageResourceNode AnimationGroupNode]
        (ds/connect @source-node-ref source-label @target-node-ref :images)

        [AnimationGroupNode AnimationGroupNode]
        (let [parent-node (ds/node-consuming @target-node-ref :animation)]
          (paste-child-command (assoc context :target-node-ref (t/node-ref parent-node))))))))

(defn disconnect-to-delete-command
  [{:keys [source-node-ref source-label target-node-ref target-label] :as context}]
  (ds/transactional (ds/disconnect @source-node-ref source-label @target-node-ref target-label)))

(defn add-image
  [evt]
  (when-let [target (first (filter #(= AtlasNode (node-type %)) (sel/selected-nodes evt)))]
    (let [project-node  (p/project-enclosing target)
          images-to-add (p/select-resources project-node ["png" "jpg"] "Add Image(s)" true)]
      (ds/transactional
        (ds/tx-label "Add Images")
        (doseq [img images-to-add]
          (ds/connect img :content target :images))))))

(defcommand add-image-command
  "com.dynamo.cr.menu-items.scene"
  "com.dynamo.cr.builtins.editors.atlas.add-image"
  "Add Image")

(defhandler add-image-handler add-image-command #'add-image)

(when (ds/in-transaction?)
  (p/register-editor "atlas" #'on-edit)
  (p/register-node-type "atlas" AtlasNode))
