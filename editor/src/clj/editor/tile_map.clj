(ns editor.tile-map
  (:require
    ;; switch to released version once https://dev.clojure.org/jira/browse/DIMAP-15 has been fixed
   [clojure.data.int-map-fixed :as int-map]
   [clojure.string :as s]
   [dynamo.graph :as g]
   [editor.colors :as colors]
   [editor.core :as core]
   [editor.defold-project :as project]
   [editor.geom :as geom]
   [editor.gl :as gl]
   [editor.gl.pass :as pass]
   [editor.gl.shader :as shader]
   [editor.gl.texture :as texture]
   [editor.gl.vertex2 :as vtx]
   [editor.graph-util :as gu]
   [editor.handler :as handler]
   [editor.material :as material]
   [editor.math :as math]
   [editor.outline :as outline]
   [editor.properties :as properties]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [editor.scene :as scene]
   [editor.scene-tools :as scene-tools]
   [editor.tile-map-grid :as tile-map-grid]
   [editor.tile-source :as tile-source]
   [editor.types :as types]
   [editor.ui :as ui]
   [editor.util :as util]
   [editor.validation :as validation]
   [editor.workspace :as workspace]
   [plumbing.core :as pc])
  (:import
   (com.dynamo.tile.proto Tile$TileGrid Tile$TileGrid$BlendMode Tile$TileLayer)
   (editor.gl.shader ShaderLifecycle)
   (com.jogamp.opengl GL GL2)
   (javax.vecmath Point3d Matrix4d Quat4d Vector3d Vector4d)))

(set! *warn-on-reflection* true)

(defn min-l ^long [^long a ^long b] (Math/min a b))
(defn max-l ^long [^long a ^long b] (Math/max a b))
(defn min-d ^double [^double a ^double b] (Math/min a b))
(defn max-d ^double [^double a ^double b] (Math/max a b))

(defn single
  [coll]
  (when-not (next coll) (first coll)))

(defn toggler
  [a b]
  (fn [v] (if (= v a) b a)))


(def tile-map-icon "icons/32/Icons_48-Tilemap.png")
(def tile-map-layer-icon "icons/32/Icons_42-Layers.png")


;; manipulating cells

(defrecord Tile [^long x ^long y ^long tile ^boolean h-flip ^boolean v-flip])

(defn cell-index ^long [^long x ^long y]
  (bit-or (bit-shift-left y Integer/SIZE)
          (bit-and x 0xFFFFFFFF)))

(defn paint-cell!
  [cell-map x y tile h-flip v-flip]
  (if tile
    (assoc! cell-map (cell-index x y) (->Tile x y tile h-flip v-flip))
    (dissoc! cell-map (cell-index x y))))

(defn make-cell-map
  [cells]
  (persistent! (reduce (fn [ret {:keys [x y tile h-flip v-flip] :or {h-flip 0 v-flip 0} :as cell}]
                         (paint-cell! ret x y tile (not= 0 h-flip) (not= 0 v-flip)))
                       (transient (int-map/int-map))
                       cells)))

(defn paint
  [cell-map [sx sy] brush]
  (let [{:keys [width height tiles]} brush]
    (let [ex (+ sx width)
          ey (+ sy height)]
      (loop [x sx
             y sy
             tiles tiles
             cell-map (transient cell-map)]
        (if (< y ey)
          (if (< x ex)
            (let [{:keys [tile h-flip v-flip]} (first tiles)]
              (recur (inc x) y (rest tiles) (paint-cell! cell-map x y tile h-flip v-flip)))
            (recur sx (inc y) tiles cell-map))
          (persistent! cell-map))))))

(defn make-brush
  [tile]
  {:width 1 :height 1 :tiles [{:tile tile :h-flip false :v-flip false}]})

(defn make-brush-from-selection
  [cell-map start-cell end-cell]
  (let [[sx sy] start-cell
        [ex ey] end-cell
        x0 (min-l sx ex)
        y0 (min-l sy ey)
        x1 (inc (max-l sx ex))
        y1 (inc (max-l sy ey))
        w (- x1 x0)
        h (- y1 y0)]
    {:width w
     :height h
     :tiles (vec (for [y (range y0 y1)
                       x (range x0 x1)]
                   (get cell-map (cell-index x y))))}))

(def erase-brush (make-brush nil))


;;--------------------------------------------------------------------
;; rendering

(vtx/defvertex pos-uv-vtx
  (vec3 position)
  (vec2 texcoord0))

(defn render-layer
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/transparent
      (let [{:keys [selected ^Matrix4d world-transform user-data]}  (first renderables)
            {:keys [node-id vbuf shader gpu-texture blend-mode]} user-data]
        (when vbuf
          (let [vertex-binding (vtx/use-with node-id vbuf shader)]
            (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
              (gl/set-blend-mode gl blend-mode)
              ;; TODO: can't use selected because we also need to know when nothing is selected
              #_(if selected
                  (shader/set-uniform shader gl "tint" (Vector4d. 1.0 1.0 1.0 1.0))
                  (shader/set-uniform shader gl "tint" (Vector4d. 1.0 1.0 1.0 0.5)))
              (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))

      pass/selection
      (let [{:keys [^Matrix4d world-transform user-data]} (first renderables)
            {:keys [node-id vbuf shader]} user-data]
        (when vbuf
          (let [vertex-binding (vtx/use-with node-id vbuf shader)]
            (gl/with-gl-bindings gl render-args [shader vertex-binding]
              (gl/gl-push-matrix gl
                (gl/gl-mult-matrix-4d gl world-transform)
                (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))))))

(defn make-tile-uv-lookup-cache
  [tile-count uv-transforms]
  (let [uv-cache (object-array tile-count)]
    (fn [tile]
      (if (< tile tile-count)
        (or (aget uv-cache tile)
            (let [[[u0 v0] [u1 v1]] (geom/uv-trans (nth uv-transforms tile) [[0 0] [1 1]])
                  uv-coords (float-array 4 [u0 v0 u1 v1])]
              (aset uv-cache tile uv-coords)
              uv-coords))
        (float-array [0 1 1 0])))))

(defn gen-layer-render-data
  [cell-map texture-set-data]
  (let [{:keys [^long tile-width ^long tile-height ^long tile-count]
         :or {tile-width 0, tile-height 0, tile-count 0}} (:texture-set texture-set-data)
        uv-lookup (make-tile-uv-lookup-cache tile-count (:uv-transforms texture-set-data))]
    (loop [^java.util.Iterator it (.iterator (.values ^java.util.Map cell-map))
           vbuf (->pos-uv-vtx (* 6 (count cell-map)))
           min-x 0 min-y 0 max-x 0 max-y 0]
      (if (.hasNext it)
        (let [^Tile tile (.next it)
              x0 (* (.x tile) tile-width)
              y0 (* (.y tile) tile-height)
              x1 (+ x0 tile-width)
              y1 (+ y0 tile-height)
              ^floats uvs (uv-lookup (.tile tile))
              u0 (aget uvs (if (.h-flip tile) 2 0))
              v0 (aget uvs (if (.v-flip tile) 3 1))
              u1 (aget uvs (if (.h-flip tile) 0 2))
              v1 (aget uvs (if (.v-flip tile) 1 3))]
          (recur it
                 (-> vbuf
                     (pos-uv-vtx-put! x0 y0 0 u0 v1)
                     (pos-uv-vtx-put! x0 y1 0 u0 v0)
                     (pos-uv-vtx-put! x1 y1 0 u1 v0)
                     (pos-uv-vtx-put! x1 y0 0 u1 v1))
                 (min-l min-x x0)
                 (min-l min-y y0)
                 (max-l max-x x1)
                 (max-l max-y y1)))
        {:vbuf (vtx/flip! vbuf)
         :aabb (-> (geom/null-aabb)
                   (geom/aabb-incorporate min-x min-y 0)
                   (geom/aabb-incorporate max-x max-y 0))}))))

(g/defnk produce-layer-scene
  [_node-id cell-map texture-set-data z gpu-texture shader blend-mode visible]
  (when visible
    (let [{:keys [aabb vbuf]} (gen-layer-render-data cell-map texture-set-data)]
      {:node-id _node-id
       :aabb aabb
       :renderable {:render-fn render-layer
                    :user-data {:node-id _node-id
                                :vbuf vbuf
                                :gpu-texture gpu-texture
                                :shader shader
                                :blend-mode blend-mode}
                    :index z
                    :passes [pass/transparent pass/selection]}})))

(g/defnk produce-layer-outline
  [_node-id id]
  {:node-id _node-id
   :label id
   :icon tile-map-layer-icon})

(g/defnk produce-layer-pb-msg
  [id z visible cell-map]
  {:id id
   :z z
   :is-visible (if visible 1 0)
   :cell (mapv (fn [{:keys [x y tile v-flip h-flip]}]
                 {:x x
                  :y y
                  :tile tile
                  :v-flip (if v-flip 1 0)
                  :h-flip (if h-flip 1 0)})
               (vals cell-map))})

(g/defnode LayerNode
  (inherits outline/OutlineNode)

  (input texture-set-data g/Any)
  (input gpu-texture g/Any)
  (input shader ShaderLifecycle)
  (input blend-mode g/Any)

  (property cell-map g/Any
            (dynamic visible (g/constantly false)))

  (property id g/Str)
  (property z g/Num
            (dynamic error (validation/prop-error-fnk :warning validation/prop-1-1? z)))

  (property visible g/Bool)

  (output scene g/Any :cached produce-layer-scene)
  (output node-outline outline/OutlineData :cached produce-layer-outline)
  (output pb-msg g/Any :cached produce-layer-pb-msg))

(defn attach-layer-node
  [parent layer-node]
  (concat
   (g/connect layer-node :_node-id                     parent :nodes)
   (g/connect layer-node :id                           parent :layer-ids)
   (g/connect layer-node :node-outline                 parent :child-outlines)
   (g/connect layer-node :scene                        parent :child-scenes)
   (g/connect layer-node :pb-msg                       parent :layer-msgs)
   (g/connect parent     :texture-set-data             layer-node :texture-set-data)
   (g/connect parent     :material-shader              layer-node :shader)
   (g/connect parent     :gpu-texture                  layer-node :gpu-texture)
   (g/connect parent     :blend-mode                   layer-node :blend-mode)))

(defn make-layer-node
  [parent tile-layer]
  (let [graph-id (g/node-id->graph-id parent)]
    (g/make-nodes
     graph-id
     [layer-node [LayerNode {:id (:id tile-layer)
                             :z (:z tile-layer)
                             :visible (not= 0 (:is-visible tile-layer))
                             :cell-map (make-cell-map (:cell tile-layer))}]]
     (attach-layer-node parent layer-node))))


(defn world-pos->tile
  [^Point3d pos tile-width tile-height]
  [(long (Math/floor (/ (.x pos) tile-width)))
   (long (Math/floor (/ (.y pos) tile-height)))])


(defn load-tile-map
  [project self resource tile-grid]
  (let [tile-source (workspace/resolve-resource resource (:tile-set tile-grid))
        material (workspace/resolve-resource resource (:material tile-grid))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (g/set-property self
                      :tile-source tile-source
                      :material material
                      :blend-mode (:blend-mode tile-grid))
      (for [tile-layer (:layers tile-grid)]
        (make-layer-node self tile-layer)))))


(g/defnk produce-scene
  [_node-id child-scenes]
  {:node-id  _node-id
   :aabb     (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes))
   :children child-scenes})

(g/defnk produce-node-outline
  [_node-id child-outlines]
  {:node-id  _node-id
   :label    "Tile Map"
   :icon     tile-map-icon
   :children (vec (sort-by :label child-outlines))})

(g/defnk produce-pb-msg
  [tile-source material blend-mode layer-msgs]
  {:tile-set   (resource/resource->proj-path tile-source)
   :material   (resource/resource->proj-path material)
   :blend-mode blend-mode
   :layers     (sort-by :z layer-msgs)})

(defn build-tile-map
  [resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes Tile$TileGrid pb-msg)}))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- prop-tile-source-range-error
  [_node-id tile-source tile-count max-tile-index]
  (validation/prop-error :fatal _node-id :tile-source
                         (fn [v name]
                           (when-not (< max-tile-index tile-count)
                             (format "Tile map uses tiles outside the range of this tile source (%d tiles in source, but a tile with index %d is used in tile map)" tile-count max-tile-index))) tile-source "Tile Source"))

(g/defnk produce-build-targets
  [_node-id resource tile-source material pb-msg dep-build-targets tile-count max-tile-index]
    (g/precluding-errors
      [(prop-resource-error :fatal _node-id :tile-source tile-source "Tile Source")
       (prop-tile-source-range-error _node-id tile-source tile-count max-tile-index)]
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-resource (into {} (map (juxt (comp :resource :resource) :resource) dep-build-targets))
            dep-resources (map (fn [[label resource]]
                                 [label (get deps-by-resource resource)])
                               [[:tile-set tile-source]
                                [:material material]])]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-tile-map
          :user-data {:pb-msg pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets}])))

(g/defnode TileMapNode
  (inherits resource-node/ResourceNode)

  (input layer-ids g/Any :array)
  (input layer-msgs g/Any :array)
  (input child-scenes g/Any :array)
  (input dep-build-targets g/Any :array)
  (input tile-source-resource resource/Resource)
  (input tile-source-attributes g/Any)
  (input texture-set-data g/Any)
  (input gpu-texture g/Any)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)

  ;; tile source
  (property tile-source resource/Resource
            (value (gu/passthrough tile-source-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :tile-source-resource]
                                            [:build-targets :dep-build-targets]
                                            [:tile-source-attributes :tile-source-attributes]
                                            [:texture-set-data :texture-set-data]
                                            [:gpu-texture :gpu-texture])))
            (dynamic error (g/fnk [_node-id tile-source tile-count max-tile-index]
                             (or (prop-resource-error :fatal _node-id :tile-source tile-source "Tile Source")
                                 (prop-tile-source-range-error _node-id tile-source tile-count max-tile-index))))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "tilesource"})))

  ;; material
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :material-resource]
                                            [:build-targets :dep-build-targets]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"})))

  (property blend-mode g/Any
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Tile$TileGrid$BlendMode))))

  (output max-tile-index g/Any :cached (g/fnk [pb-msg]
                                         (transduce (comp (mapcat :cell)
                                                          (map :tile))
                                                    max 0 (:layers pb-msg))))

  (output tile-source-attributes g/Any (gu/passthrough tile-source-attributes))
  (output tile-count g/Int (g/fnk [tile-source-attributes]
                             (* (:tiles-per-row tile-source-attributes 0) (:tiles-per-column tile-source-attributes 0))))
  (output tile-dimensions g/Any
          (g/fnk [tile-source-attributes]
            (when tile-source-attributes
              (let [{:keys [width height]} tile-source-attributes]
                (when (and width height)
                  [width height])))))

  (output texture-set-data g/Any (gu/passthrough texture-set-data))
  (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
                             (or (some-> material-samplers first material/sampler->tex-params)
                                 default-tex-params)))
  (output gpu-texture g/Any (g/fnk [gpu-texture tex-params] (texture/set-params gpu-texture tex-params)))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-node-outline)
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))


;;--------------------------------------------------------------------
;; tool

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

(def tex-shader (shader/make-shader ::tex-shader pos-uv-vert pos-uv-frag))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader pos-color-vert
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader pos-color-frag
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def color-shader (shader/make-shader ::color-shader pos-color-vert pos-color-frag))



;; brush

(defn render-brush-outline
  [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        [x y] (:cell user-data)
        {:keys [width height]} (:brush user-data)
        [tile-width tile-height] (:tile-dimensions user-data)]
    (when (and x y)
      (let [x0 (* tile-width x)
            y0 (* tile-height y)
            x1 (+ x0 (* width tile-width))
            y1 (+ y0 (* height tile-height))
            z 0.0
            c (double-array (map #(/ % 255.0) [255 255 255]))]
        (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
        (.glBegin gl GL2/GL_LINE_LOOP)
        (.glVertex3d gl x0 y0 z)
        (.glVertex3d gl x1 y0 z)
        (.glVertex3d gl x1 y1 z)
        (.glVertex3d gl x0 y1 z)
        (.glEnd gl)))))


(defn conj-brush-quad!
  [vbuf {:keys [tile h-flip v-flip]} uvs w h x y]
  (if-not tile
    vbuf
    (let [uv (nth uvs tile (geom/identity-uv-trans))
          [p1 p2] (geom/uv-trans uv [[0 0] [1 1]])
          u0 (first (if h-flip p2 p1))
          v0 (second (if v-flip p2 p1))
          u1 (first (if h-flip p1 p2))
          v1 (second (if v-flip p1 p2))
          x0 x
          y0 y
          x1 (+ x0 w)
          y1 (+ y0 h)]
      (-> vbuf
          (pos-uv-vtx-put! x0 y0 0 u0 v1)
          (pos-uv-vtx-put! x0 y1 0 u0 v0)
          (pos-uv-vtx-put! x1 y1 0 u1 v0)
          (pos-uv-vtx-put! x1 y0 0 u1 v1)))))

(defn gen-brush-vbuf
  [brush uvs tile-width tile-height]
  (let [{:keys [width height tiles]} brush]
    (loop [x 0
           y 0
           tiles tiles
           vbuf (->pos-uv-vtx (* 4 (count tiles)))]
      (if (< y height)
        (if (< x width)
          (recur (inc x) y (rest tiles) (conj-brush-quad! vbuf (first tiles) uvs tile-width tile-height (* x tile-width) (* y tile-height)))
          (recur 0 (inc y) tiles vbuf))
        (vtx/flip! vbuf)))))

(defn render-brush
  [^GL2 gl render-args renderables n]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        {:keys [texture-set-data gpu-texture brush]} user-data
        uvs (:uv-transforms texture-set-data)
        [x y] (:cell user-data)
        [w h] (:tile-dimensions user-data)
        vbuf (gen-brush-vbuf brush uvs w h)
        vb (vtx/use-with ::brush vbuf tex-shader)
        brush-transform (doto (Matrix4d. geom/Identity4d)
                          (.set (Vector3d. (* x w) (* y h) 0)))]
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (gl/gl-push-matrix gl
                       (gl/gl-mult-matrix-4d gl brush-transform)
                       (gl/with-gl-bindings gl render-args [gpu-texture tex-shader vb]
                         (shader/set-uniform tex-shader gl "texture" 0)
                         (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))


;; palette

(def ^:const tile-border-size 0.5)
(def ^:const ideal-tile-size 64)

(defn- make-palette-transform
  [tile-source-attributes viewport]
  (let [{:keys [width height visual-width visual-height]} tile-source-attributes
        {:keys [bottom right]} viewport
        ratio (max-d (/ visual-width right)
                     (/ visual-height bottom))
        zoom (+ ratio 0.2)
        max-zoom (max-d 1.0 (/ ideal-tile-size (max-l width height)))
        scale (min-d (/ 1.0 zoom) max-zoom)
        scale-m (doto (Matrix4d.)
                  (.setIdentity)
                  (.set (Vector3d. (* 0.5 right)
                                   (* 0.5 bottom)
                                   0.0))
                  (.setScale scale))
        trans-m (doto (Matrix4d.)
                  (.set (Vector3d. (- (* 0.5 visual-width))
                                   (- (* 0.5 visual-height))
                                   0.0)))]
    (doto scale-m
      (.mul trans-m))))

(g/defnk produce-palette-tile
  [^Vector3d cursor-screen-pos
   tile-source-attributes
   tile-dimensions
   ^Matrix4d palette-transform]
  (when (and cursor-screen-pos tile-source-attributes)
    (let [{:keys [width height tiles-per-row tiles-per-column]} tile-source-attributes
          p (Point3d. cursor-screen-pos)
          t (doto (Matrix4d. palette-transform)
              (.invert))
          _ (.transform t p)
          [x y] (world-pos->tile p
                                 (+ width tile-border-size)
                                 (+ height tile-border-size))]
      (when (and (<= 0 x (dec tiles-per-row))
                 (<= 0 y (dec tiles-per-column)))
        (+ (* y tiles-per-row) x)))))

(defn gen-palette-tiles-vbuf
  [tile-source-attributes texture-set-data]
  (let [uvs (:uv-transforms texture-set-data)
        w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        vbuf (->pos-uv-vtx (* 4 (:tile-count (:texture-set texture-set-data))))]
    (loop [x 0
           y 0
           vbuf vbuf]
      (if (< y rows)
        (if (< x cols)
          (let [uv (nth uvs (+ x (* y cols)))
                x0 (+ (* x (+ tile-border-size w)) tile-border-size)
                x1 (+ x0 w)
                y0 (+ (* y (+ tile-border-size h)) tile-border-size)
                y1 (+ y0 h)
                [[u0 v0] [u1 v1]] (geom/uv-trans uv [[0 0] [1 1]])]
            (recur (inc x)
                   y
                   (-> vbuf
                       (pos-uv-vtx-put! x0 y0 0 u0 v0)
                       (pos-uv-vtx-put! x0 y1 0 u0 v1)
                       (pos-uv-vtx-put! x1 y1 0 u1 v1)
                       (pos-uv-vtx-put! x1 y0 0 u1 v0))))
          (recur 0 (inc y) vbuf))
        (vtx/flip! vbuf)))))

(defn- render-palette-tiles
  [^GL2 gl render-args tile-source-attributes texture-set-data gpu-texture]
  (let [vbuf (gen-palette-tiles-vbuf tile-source-attributes texture-set-data)
        vb (vtx/use-with ::palette-tiles vbuf tex-shader)
        gpu-texture (texture/set-params gpu-texture tile-source/texture-params)]
    (gl/with-gl-bindings gl render-args [gpu-texture tex-shader vb]
      (shader/set-uniform tex-shader gl "texture" 0)
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))


(defn gen-palette-grid-vbuf
  [tile-source-attributes]
  (let [tw (:width tile-source-attributes)
        th (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        w (+ (:visual-width tile-source-attributes) (* cols tile-border-size))
        h (+ (:visual-height tile-source-attributes) (* rows tile-border-size))]
    (as-> (->color-vtx (+ (* (+ 1 rows) 2)
                          (* (+ 1 cols) 2)))
        vbuf
      (reduce (fn [vbuf y]
                (let [y0 (* y (+ th tile-border-size))]
                  (-> vbuf
                      (color-vtx-put! 0 y0 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! w y0 0 0.3 0.3 0.3 1.0)))) vbuf (range (inc rows)))
      (reduce (fn [vbuf x]
                (let [x0 (* x (+ tw tile-border-size))]
                  (-> vbuf
                      (color-vtx-put! x0 0 0 0.3 0.3 0.3 1.0)
                      (color-vtx-put! x0 h 0 0.3 0.3 0.3 1.0)))) vbuf (range (inc cols)))
      (vtx/flip! vbuf))))

(defn- render-palette-grid
  [^GL2 gl render-args tile-source-attributes]
  (let [vbuf (gen-palette-grid-vbuf tile-source-attributes)
        vb (vtx/use-with ::palette-grid vbuf color-shader)]
    (gl/with-gl-bindings gl render-args [color-shader vb]
      (gl/gl-draw-arrays gl GL2/GL_LINES 0 (count vbuf)))))

(defn- render-palette-active
  [^GL2 gl render-args tile-source-attributes palette-tile]
  (when palette-tile
    (let [n palette-tile
          w (:width tile-source-attributes)
          h (:height tile-source-attributes)
          x (mod n (:tiles-per-row tile-source-attributes))
          y (int (/ n (:tiles-per-row tile-source-attributes)))
          x0 (* x (+ tile-border-size w))
          x1 (+ x0 w tile-border-size)
          y0 (* y (+ tile-border-size h))
          y1 (+ y0 h tile-border-size)
          vbuf (-> (->color-vtx 4)
                   (color-vtx-put! x0 y0 0 1.0 1.0 1.0 1.0)
                   (color-vtx-put! x0 y1 0 1.0 1.0 1.0 1.0)
                   (color-vtx-put! x1 y1 0 1.0 1.0 1.0 1.0)
                   (color-vtx-put! x1 y0 0 1.0 1.0 1.0 1.0)
                   (vtx/flip!))
          vb (vtx/use-with ::palette-active vbuf color-shader)]
      (gl/with-gl-bindings gl render-args [color-shader vb]
        (gl/gl-draw-arrays gl GL2/GL_LINE_LOOP 0 (count vbuf))))))

(defn render-palette-background
  [^GL2 gl viewport]
  (let [{:keys [top left right bottom]} viewport]
    (.glColor4d gl 0.0 0.0 0.0 0.7)
    (.glBegin gl GL2/GL_QUADS)
    (.glVertex2d gl 0.0 0.0)
    (.glVertex2d gl right 0.0)
    (.glVertex2d gl right bottom)
    (.glVertex2d gl 0.0 bottom)
    (.glEnd gl)))

(defn render-palette
  [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        {:keys [viewport tile-source-attributes texture-set-data gpu-texture palette-transform palette-tile]} user-data]
    (render-palette-background gl viewport)
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (gl/gl-push-matrix gl
                       (gl/gl-mult-matrix-4d gl palette-transform)
                       (render-palette-tiles gl render-args tile-source-attributes texture-set-data gpu-texture)
                       (render-palette-grid gl render-args tile-source-attributes)
                       (render-palette-active gl render-args tile-source-attributes palette-tile))))


(defn render-editor-select-outline
  [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        [sx sy] (:start user-data)
        [ex ey] (:end user-data)
        [tile-width tile-height] (:tile-dimensions user-data)]
    (when (and sx sy ex ey)
      (let [x0 (* tile-width (min-l sx ex))
            y0 (* tile-height (min-l sy ey))
            x1 (* tile-width (inc (max-l sx ex)))
            y1 (* tile-height (inc (max-l sy ey)))
            z 0.0
            c (double-array (map #(/ % 255.0) [255 255 255]))]
        (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
        (.glBegin gl GL2/GL_LINE_LOOP)
        (.glVertex3d gl x0 y0 z)
        (.glVertex3d gl x1 y0 z)
        (.glVertex3d gl x1 y1 z)
        (.glVertex3d gl x0 y1 z)
        (.glEnd gl)))))

(defn render-editor-select
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/outline
      (render-editor-select-outline gl render-args renderables n))))

(defn render-editor
  [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/transparent
      (render-brush gl render-args renderables n)

      pass/outline
      (render-brush-outline gl render-args renderables n))))

(g/defnk produce-palette-renderables
  [viewport tile-source-attributes texture-set-data gpu-texture palette-transform palette-tile]
  {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                  :render-fn render-palette
                  :user-data {:viewport viewport
                              :tile-source-attributes tile-source-attributes
                              :texture-set-data texture-set-data
                              :gpu-texture gpu-texture
                              :palette-transform palette-transform
                              :palette-tile palette-tile}}]})

(g/defnk produce-editor-renderables
  [active-layer op op-select-start op-select-end current-tile tile-dimensions brush viewport texture-set-data gpu-texture]
  (when active-layer
    {pass/transparent (cond
                        (= :select op)
                        []

                        current-tile
                        [{:world-transform (Matrix4d. geom/Identity4d)
                          :render-fn render-editor
                          :user-data {:cell current-tile
                                      :brush brush
                                      :tile-dimensions tile-dimensions
                                      :texture-set-data texture-set-data
                                      :gpu-texture gpu-texture}}])
     pass/outline     (cond
                        (= :select op) [{:world-transform (Matrix4d. geom/Identity4d)
                                         :render-fn render-editor-select
                                         :user-data {:start op-select-start
                                                     :end op-select-end
                                                     :tile-dimensions tile-dimensions}}]
                        current-tile
                        [{:world-transform (Matrix4d. geom/Identity4d)
                          :render-fn render-editor
                          :user-data {:cell current-tile
                                      :brush brush
                                      :tile-dimensions tile-dimensions}}])}))

(g/defnk produce-tool-renderables
  [mode editor-renderables palette-renderables]
  (case mode
    :editor editor-renderables
    :palette palette-renderables))


;;--------------------------------------------------------------------
;; input handling

(defmulti begin-op (fn [op node action state evaluation-context] op))
(defmulti update-op (fn [op node action state evaluation-context] op))
(defmulti end-op (fn [op node action state evaluation-context] op))

;; painting tiles from brush

(defmethod begin-op :paint
  [op self action state evaluation-context]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      (let [brush (g/node-value self :brush evaluation-context)
            op-seq (gensym)]
        (swap! state assoc :last-tile current-tile)
        [(g/set-property self :op-seq op-seq)
         (g/operation-sequence op-seq)
         (g/update-property active-layer :cell-map paint current-tile brush)]))))

(defmethod update-op :paint
  [op self action state evaluation-context]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      (when (not= current-tile (-> state deref :last-tile))
        (swap! state assoc :last-tile current-tile)
        (let [brush (g/node-value self :brush evaluation-context)
              op-seq (g/node-value self :op-seq evaluation-context)]
          [(g/operation-sequence op-seq)
           (g/update-property active-layer :cell-map paint current-tile brush)])))))

(defmethod end-op :paint
  [op self action state evaluation-context]
  (swap! state dissoc :last-tile)
  [(g/set-property self :op-seq nil)])


;; selecting brush from cell-map

(defmethod begin-op :select
  [op self action state evaluation-context]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      [(g/set-property self :op-select-start current-tile)
       (g/set-property self :op-select-end current-tile)])))

(defmethod update-op :select
  [op self action state evaluation-context]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (when-let [current-tile (g/node-value self :current-tile evaluation-context)]
      [(g/set-property self :op-select-end current-tile)])))

(defmethod end-op :select
  [op self action state evaluation-context]
  (when-let [active-layer (g/node-value self :active-layer evaluation-context)]
    (let [cell-map (g/node-value active-layer :cell-map evaluation-context)
          start (g/node-value self :op-select-start evaluation-context)
          end (g/node-value self :op-select-end evaluation-context)]
      [(g/set-property self :brush (make-brush-from-selection cell-map start end))
       (g/set-property self :op-select-start nil)
       (g/set-property self :op-select-end nil)])))


(defn- handle-input-editor
  [self action state evaluation-context]
  (let [op (g/node-value self :op evaluation-context)
        tx (case (:type action)
             :mouse-pressed  (when-not (some? op)
                               (let [op (if (true? (:shift action))
                                          :select
                                          :paint)
                                     op-tx (begin-op op self action state evaluation-context)]
                                 (when (seq op-tx)
                                   (concat
                                     (g/set-property self :op op)
                                     op-tx))))

             :mouse-moved    (concat
                              (g/set-property self :cursor-world-pos (:world-pos action))
                              (when (some? op)
                                (update-op op self action state evaluation-context)))

             :mouse-released (when (some? op)
                               (concat
                                (g/set-property self :op nil)
                                (end-op op self action state evaluation-context)))
             nil)]
    (when (seq tx)
      (g/transact tx)
      true)))



(defn- handle-input-palette
  [self action state evaluation-context]
  (let [^Point3d screen-pos (:screen-pos action)]
    (case (:type action)
      :mouse-pressed  true
      :mouse-moved    (g/transact
                       (g/set-property self :cursor-screen-pos screen-pos))
      :mouse-released (let [palette-tile (g/node-value self :palette-tile evaluation-context)]
                        (g/transact
                         (concat
                          (g/set-property self :brush (make-brush palette-tile))
                          (g/set-property self :mode :editor))))

      action)))

(defn handle-input
  [self action state]
  (let [evaluation-context (g/make-evaluation-context)
        mode (g/node-value self :mode evaluation-context)]
    (case mode
      :palette (handle-input-palette self action state evaluation-context)
      :editor  (handle-input-editor self action state evaluation-context))))

(defn make-input-handler
  []
  (let [state (atom nil)]
    (fn [self action _]
      (handle-input self action state))))

(g/defnode TileMapController
  (property prefs g/Any)
  (property cursor-world-pos Point3d)
  (property cursor-screen-pos Vector3d)

  (property mode g/Keyword (default :editor))

  (property op g/Keyword)
  (property op-seq g/Any)
  (property op-select-start g/Any)
  (property op-select-end g/Any)

  (property brush g/Any (default (make-brush 0)))

  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  (input tile-source-attributes g/Any)
  (input texture-set-data g/Any)
  (input material-shader ShaderLifecycle)
  (input gpu-texture g/Any)
  (input tile-dimensions g/Any)

  (output active-layer g/Any
          (g/fnk [selected-renderables]
            (->> selected-renderables
                 (filter #(g/node-instance? LayerNode (:node-id %)))
                 single
                 :node-id)))

  (output current-tile g/Any
          (g/fnk [cursor-world-pos tile-dimensions]
            (when (and cursor-world-pos tile-dimensions)
              (let [[w h] tile-dimensions]
                (world-pos->tile cursor-world-pos w h)))))

  (output palette-transform g/Any
          (g/fnk [tile-source-attributes viewport]
            (when tile-source-attributes
              (make-palette-transform tile-source-attributes viewport))))

  (output palette-tile g/Any produce-palette-tile)
  (output editor-renderables pass/RenderData produce-editor-renderables)
  (output palette-renderables pass/RenderData produce-palette-renderables)
  (output renderables pass/RenderData :cached produce-tool-renderables)
  (output input-handler Runnable :cached (g/constantly (make-input-handler))))

(defmethod scene/attach-tool-controller ::TileMapController
  [_ tool-id view-id resource-id]
  (concat
   (g/connect resource-id :tile-source-attributes tool-id :tile-source-attributes)
   (g/connect resource-id :texture-set-data tool-id :texture-set-data)
   (g/connect resource-id :material-shader tool-id :material-shader)
   (g/connect resource-id :gpu-texture tool-id :gpu-texture)
   (g/connect resource-id :tile-dimensions tool-id :tile-dimensions)))


;; handlers/menu

(defn- selection->tile-map [selection]
  (handler/adapt-single selection TileMapNode))

(defn- selection->layer [selection]
  (handler/adapt-single selection LayerNode))

(defn tile-map-node
  [selection]
  (or (selection->tile-map selection)
      (some-> (selection->layer selection)
        core/scope)))

(defn- gen-unique-name
  [basename existing-names]
  (let [existing-names (set existing-names)]
    (loop [postfix 0]
      (let [name (if (= postfix 0) basename (str basename postfix))]
        (if (existing-names name)
          (recur (inc postfix))
          name)))))

(defn- make-new-layer
  [id]
  (assoc (protobuf/pb->map (Tile$TileLayer/getDefaultInstance))
         :id id))

(defn- add-layer-handler
  [tile-map-node]
  (let [layer-ids (set (g/node-value tile-map-node :layer-ids))
        layer-id (gen-unique-name "layer" layer-ids)]
    (g/transact
     (concat
      (g/operation-label "Add layer")
      (make-layer-node tile-map-node (make-new-layer layer-id))))))

(handler/defhandler :add :workbench
  (label [user-data] "Add layer")
  (active? [selection] (selection->tile-map selection))
  (run [selection user-data] (add-layer-handler (selection->tile-map selection))))

(defn- erase-tool-handler [tool-controller]
  (g/set-property! tool-controller :brush erase-brush))

(defn- active-tile-map [app-view]
  (when-let [resource-node (g/node-value app-view :active-resource-node)]
    (when (g/node-instance? TileMapNode resource-node)
      resource-node)))

(defn- active-scene-view [app-view]
  (when-let [view-node (g/node-value app-view :active-view)]
    (when (g/node-instance? scene/SceneView view-node)
      view-node)))

(defn- scene-view->tool-controller [scene-view]
  ;; TODO Hack, but better than before
  (let [input-handlers (map first (g/sources-of scene-view :input-handlers))]
    (first (filter (partial g/node-instance? TileMapController) input-handlers))))

(handler/defhandler :erase-tool :workbench
  (label [user-data] "Select Eraser")
  (active? [app-view] (and (active-tile-map app-view)
                        (active-scene-view app-view)))
  (enabled? [app-view selection]
    (and (selection->layer selection)
         (-> (active-tile-map app-view)
             (g/node-value :tile-source-resource))))
  (run [app-view] (erase-tool-handler (-> (active-scene-view app-view) scene-view->tool-controller))))

(defn- tile-map-palette-handler [tool-controller]
  (g/update-property! tool-controller :mode (toggler :palette :editor)))

(handler/defhandler :show-palette :workbench
  (active? [app-view] (and (active-tile-map app-view)
                        (active-scene-view app-view)))
  (enabled? [app-view selection]
    (and (selection->layer selection)
         (-> (active-tile-map app-view)
             (g/node-value :tile-source-resource))))
  (run [app-view] (tile-map-palette-handler (-> (active-scene-view app-view) scene-view->tool-controller))))

(ui/extend-menu ::menubar :editor.app-view/edit-end
                [{:label   "Select Tile..."
                  :command :show-palette}
                 {:label   "Select Eraser"
                  :command :erase-tool}])

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext ["tilemap" "tilegrid"]
    :build-ext "tilegridc"
    :node-type TileMapNode
    :ddf-type Tile$TileGrid
    :load-fn load-tile-map
    :icon tile-map-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid tile-map-grid/TileMapGrid
                        :tool-controller TileMapController}}
    :tags #{:component :non-embeddable}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}
    :label "Tile Map"))
