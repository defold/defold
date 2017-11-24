(ns editor.tile-source
  (:require
   [clojure.string :as str]
   [dynamo.graph :as g]
   [editor.app-view :as app-view]
   [editor.camera :as camera]
   [editor.collision-groups :as collision-groups]
   [editor.graph-util :as gu]
   [editor.image :as image]
   [editor.image-util :as image-util]
   [editor.workspace :as workspace]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [editor.defold-project :as project]
   [editor.handler :as handler]
   [editor.gl :as gl]
   [editor.gl.shader :as shader]
   [editor.gl.texture :as texture]
   [editor.gl.vertex :as vtx]
   [editor.geom :as geom]
   [editor.types :as types]
   [editor.gl.pass :as pass]
   [editor.pipeline.tex-gen :as tex-gen]
   [editor.pipeline.texture-set-gen :as texture-set-gen]
   [editor.properties :as properties]
   [editor.scene :as scene]
   [editor.texture-set :as texture-set]
   [editor.outline :as outline]
   [editor.protobuf :as protobuf]
   [editor.util :as util]
   [editor.validation :as validation])
  (:import
   [com.dynamo.tile.proto Tile$TileSet Tile$Playback]
   [com.dynamo.textureset.proto TextureSetProto$TextureSet]
   [com.google.protobuf ByteString]
   [editor.types AABB]
   [javax.vecmath Point3d Matrix4d Vector3d]
   [com.jogamp.opengl GL GL2]
   [java.awt.image BufferedImage]
   [java.nio ByteBuffer ByteOrder FloatBuffer]))

(set! *warn-on-reflection* true)

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))

(defn single
  [coll]
  (when-not (next coll) (first coll)))

(def tile-source-icon "icons/32/Icons_47-Tilesource.png")
(def animation-icon "icons/32/Icons_24-AT-Animation.png")
(def collision-icon "icons/32/Icons_43-Tilesource-Collgroup.png")

(def texture-params
  {:min-filter gl/nearest
   :mag-filter gl/nearest
   :wrap-s     gl/clamp
   :wrap-t     gl/clamp})

(vtx/defvertex pos-uv-vtx
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

(def tile-shader (shader/make-shader ::tile-shader pos-uv-vert pos-uv-frag))

(vtx/defvertex pos-color-vtx
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

(def tile-border-size 3.0)

(defn- tile-coords
  [tile-index tile-source-attributes [scale-x scale-y]]
  (let [w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        row (mod tile-index cols)
        col (long (/ tile-index cols))
        x-border (* scale-x tile-border-size)
        y-border (* scale-y tile-border-size)
        x0 (+ (* row (+ x-border w)) x-border)
        x1 (+ x0 w)
        y0 (+ (* (- rows col 1) (+ y-border h)) y-border)
        y1 (+ y0 h)]
    [[x0 y0] [x1 y1]]))

(defn assign-collision-group
  [tile->collision-group tile group-node-id]
  (assoc tile->collision-group tile group-node-id))

(g/defnk produce-pb
  [image tile-width tile-height tile-margin tile-spacing collision material-tag
   cleaned-convex-hulls collision-groups animation-ddfs extrude-borders inner-padding]
  {:image (resource/resource->proj-path image)
   :tile-width tile-width
   :tile-height tile-height
   :tile-margin tile-margin
   :tile-spacing tile-spacing
   :collision (resource/resource->proj-path collision)
   :material-tag material-tag
   :convex-hulls (mapv (fn [{:keys [index count collision-group]}]
                         {:index index
                          :count count
                          :collision-group (or collision-group "")}) cleaned-convex-hulls)
   :convex-hull-points (vec (mapcat :points cleaned-convex-hulls))
   :collision-groups (sort collision-groups)
   :animations (sort-by :id animation-ddfs)
   :extrude-borders extrude-borders
   :inner-padding inner-padding})

(defn- build-texture-set [resource dep-resources user-data]
  (let [tex-set (assoc (:texture-set user-data) :texture (resource/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes TextureSetProto$TextureSet tex-set)}))

(g/defnk produce-build-targets [_node-id resource packed-image texture-set texture-profile build-settings]
  (let [workspace        (project/workspace (project/get-project _node-id))
        compress?         (:compress-textures? build-settings false)
        texture-target   (image/make-texture-build-target workspace _node-id packed-image texture-profile compress?)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture-set
      :user-data {:texture-set texture-set
                  :dep-resources [[:texture (:resource texture-target)]]}
      :deps [texture-target]}]))


(g/defnk produce-anim-data [texture-set uv-transforms]
  (texture-set/make-anim-data texture-set uv-transforms))

(g/defnode CollisionGroupNode
  (inherits outline/OutlineNode)

  (property id g/Str
            (dynamic error (g/fnk [_node-id id collision-groups-data]
                                  (or (validation/prop-error :fatal _node-id :id validation/prop-empty? id "Id")
                                      (when (collision-groups/overallocated? collision-groups-data)
                                        (validation/prop-error :warning _node-id :id (constantly "More than 16 collision groups in use.") id "Id"))))))

  (input collision-groups-data g/Any)

  (output collision-group-node g/Any
          (g/fnk [_node-id id]
            {:node-id _node-id
             :collision-group id}))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id collision-groups-data]
            {:node-id _node-id
             :label id
             :icon collision-icon
             :color (collision-groups/color collision-groups-data id)})))


(defn- boolean->int [b]
  (if b 1 0))

(g/defnk produce-animation-ddf [id start-tile end-tile playback fps flip-horizontal flip-vertical cues :as all]
  (-> all
      (dissoc :_node-id :basis)
      (update :flip-horizontal boolean->int)
      (update :flip-vertical boolean->int)))

(defn- prop-tile-range? [max v name]
  (when (or (< v 1) (< max v))
    (format "%s is outside the tile range (1-%d)" name max)))

(defn render-animation
  [^GL2 gl render-args renderables n]
  (let [{:keys [camera viewport pass]} render-args
        [sx sy sz] (camera/scale-factor camera viewport)]
    (condp = pass
      pass/outline
      (doseq [renderable renderables]
        (let [state (-> renderable :updatable :state)]
          (when-let [frame (:frame state)]
            (let [user-data (:user-data renderable)
                  {:keys [start-tile tile-source-attributes]} user-data
                  [[x0 y0] [x1 y1]] (tile-coords (+ (dec start-tile) frame) tile-source-attributes [sx sy])
                  [cr cg cb ca] scene/selected-outline-color]
              (.glColor4d gl cr cg cb ca)
              (.glBegin gl GL2/GL_LINE_LOOP)
              (.glVertex3d gl x0 y0 0)
              (.glVertex3d gl x0 y1 0)
              (.glVertex3d gl x1 y1 0)
              (.glVertex3d gl x1 y0 0)
              (.glEnd gl)))))

      pass/overlay
      (texture-set/render-animation-overlay gl render-args renderables n ->pos-uv-vtx tile-shader))))

(g/defnk produce-animation-updatable
  [_node-id id anim-data]
  (texture-set/make-animation-updatable _node-id "Tile Source Animation" (get anim-data id)))

(g/defnk produce-animation-scene
  [_node-id gpu-texture updatable id anim-data tile-source-attributes start-tile]
  {:node-id    _node-id
   :aabb       (geom/null-aabb)
   :renderable {:render-fn render-animation
                :batch-key nil
                :user-data {:gpu-texture gpu-texture
                            :tile-source-attributes tile-source-attributes
                            :anim-data   (get anim-data id)
                            :start-tile  start-tile}
                :passes    [pass/outline pass/overlay pass/selection]}
   :updatable  updatable})

(g/defnode TileAnimationNode
  (inherits outline/OutlineNode)
  (property id g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? id)))
  (property start-tile g/Int
            (dynamic error (g/fnk [_node-id start-tile tile-count]
                                  (validation/prop-error :fatal _node-id :start-tile (partial prop-tile-range? tile-count) start-tile "Start Tile"))))
  (property end-tile g/Int
            (dynamic error (g/fnk [_node-id end-tile tile-count]
                                  (validation/prop-error :fatal _node-id :end-tile (partial prop-tile-range? tile-count) end-tile "End Tile"))))
  (property playback types/AnimationPlayback
            (default :playback-once-forward)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Tile$Playback))))
  (property fps g/Int (default 30)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? fps)))
  (property flip-horizontal g/Bool (default false))
  (property flip-vertical g/Bool (default false))
  (property cues g/Any (dynamic visible (g/constantly false)))

  (input tile-count g/Int)
  (input tile-source-attributes g/Any)
  (input anim-data g/Any)
  (input gpu-texture g/Any)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id id] {:node-id _node-id :label id :icon animation-icon}))
  (output ddf-message g/Any produce-animation-ddf)
  (output animation-data g/Any (g/fnk [_node-id ddf-message] {:node-id _node-id :ddf-message ddf-message}))
  (output updatable g/Any produce-animation-updatable)
  (output scene g/Any produce-animation-scene))

(defn- attach-animation-node [self animation-node]
  (concat
    (for [[from to] [[:_node-id :nodes]
                     [:node-outline :child-outlines]
                     [:ddf-message :animation-ddfs]
                     [:animation-data :animation-data]
                     [:id :animation-ids]
                     [:scene :child-scenes]]]
      (g/connect animation-node from self to))
    (for [[from to] [[:tile-count :tile-count]
                     [:tile-source-attributes :tile-source-attributes]
                     [:anim-data :anim-data]
                     [:gpu-texture :gpu-texture]]]
      (g/connect self from animation-node to))))

(defn- attach-collision-group-node
  [self collision-group-node]
  (let [project (project/get-project self)]
    (concat
     (g/connect collision-group-node :_node-id self :nodes)
     (g/connect collision-group-node :node-outline self :child-outlines)
     (g/connect collision-group-node :id self :collision-groups)
     (g/connect collision-group-node :collision-group-node project :collision-group-nodes)
     (g/connect project :collision-groups-data collision-group-node :collision-groups-data))))

(defn- outline-sort-by-fn [v]
  [(:name (g/node-type* (:node-id v)))
   (when-let [label (:label v)] (util/natural-order-key (str/lower-case label)))])

(g/defnk produce-tile-source-outline [_node-id child-outlines]
  (let [[coll-outlines anim-outlines] (let [outlines (group-by #(g/node-instance? CollisionGroupNode (:node-id %)) child-outlines)]
                                        [(get outlines true) (get outlines false)])]
    {:node-id _node-id
     :label "Tile Source"
     :icon tile-source-icon
     :children (into (outline/natural-sort coll-outlines) (outline/natural-sort anim-outlines))
     :child-reqs [{:node-type TileAnimationNode
                   :tx-attach-fn attach-animation-node}
                  {:node-type CollisionGroupNode
                   :tx-attach-fn attach-collision-group-node}]}))

(g/defnk produce-aabb
  [tile-source-attributes]
  (if tile-source-attributes
    (let [{:keys [visual-width visual-height]} tile-source-attributes]
      (types/->AABB (Point3d. 0 0 0) (Point3d. visual-width visual-height 0)))
    (geom/null-aabb)))

(defn gen-tiles-vbuf
  [tile-source-attributes uv-transforms scale]
  (let [uvs uv-transforms
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)]
    (persistent!
     (reduce (fn [vbuf tile-index]
               (let [uv (nth uvs tile-index)
                     [[x0 y0] [x1 y1]] (tile-coords tile-index tile-source-attributes scale)
                     [[u0 v0] [u1 v1]] (geom/uv-trans uv [[0 0] [1 1]])]
                 (-> vbuf
                     (conj! [x0 y0 0 1 u0 v1])
                     (conj! [x0 y1 0 1 u0 v0])
                     (conj! [x1 y1 0 1 u1 v0])
                     (conj! [x1 y0 0 1 u1 v1]))))
             (->pos-uv-vtx (* 4 rows cols))
             (range (* rows cols))))))

(defn- render-tiles
  [^GL2 gl render-args node-id gpu-texture tile-source-attributes uv-transforms scale-factor]
  (let [vbuf (gen-tiles-vbuf tile-source-attributes uv-transforms scale-factor)
        vb (vtx/use-with node-id vbuf tile-shader)
        gpu-texture (texture/set-params gpu-texture texture-params)]
    (gl/with-gl-bindings gl render-args [gpu-texture tile-shader vb]
      (shader/set-uniform tile-shader gl "texture" 0)
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))

(defn gen-tile-outlines-vbuf
  [tile-source-attributes convex-hulls scale collision-groups-data]
  (let [rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)]
    (persistent!
     (reduce (fn [vbuf tile-index]
               (let [[[x0 y0] [x1 y1]] (tile-coords tile-index tile-source-attributes scale)
                     {:keys [points collision-group]} (nth convex-hulls tile-index nil)
                     [cr cg cb ca] (if (seq points)
                                     (if collision-group
                                       (collision-groups/color collision-groups-data collision-group)
                                       [1.0 1.0 1.0 1.0])
                                     [0.15 0.15 0.15 0.15])]
                 (-> vbuf
                     (conj! [x0 y0 0 cr cg cb ca])
                     (conj! [x0 y1 0 cr cg cb ca])
                     (conj! [x1 y1 0 cr cg cb ca])
                     (conj! [x1 y0 0 cr cg cb ca]))))
             (->pos-color-vtx (* 4 rows cols))
             (range (* rows cols))))))

(defn- render-tile-outlines
  [^GL2 gl render-args node-id tile-source-attributes convex-hulls scale-factor collision-groups-data]
  (let [vbuf (gen-tile-outlines-vbuf tile-source-attributes convex-hulls scale-factor collision-groups-data)
        vb (vtx/use-with node-id vbuf color-shader)]
    (gl/with-gl-bindings gl render-args [color-shader vb]
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))

(defn conj-hull-outline!
  [vbuf points rgba]
  (reduce (fn [vbuf [x y]]
            (conj! vbuf (into [x y 0] rgba)))
          vbuf
          (interleave points (concat (drop 1 points) [(first points)]))))

(defn- translate-hull-points
  [hull-points offset-x offset-y]
  (reduce (fn [ret [x y]]
            (conj ret [(+ x offset-x)
                       (+ y offset-y)]))
          []
          (partition-all 2 hull-points)))

(defn gen-hulls-vbuf
  [tile-source-attributes convex-hulls [scale-x scale-y] collision-groups-data]
  (let [w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        x-border (* scale-x tile-border-size)
        y-border (* scale-y tile-border-size)
        npoints (transduce (map :count) + convex-hulls)]
    (persistent!
     (reduce (fn [vbuf [x y]]
               (if-let [{:keys [points collision-group]} (nth convex-hulls (+ x (* y cols)) nil)]
                 (let [offset-x (+ 0.5 (* x (+ x-border w)) x-border)
                       offset-y (+ 0.5 (* (- rows y 1) (+ y-border h)) y-border)
                       translated-points (translate-hull-points points offset-x offset-y)
                       color (collision-groups/color collision-groups-data collision-group)]
                   (conj-hull-outline! vbuf translated-points color))))
             (->pos-color-vtx (* 2 npoints))
             (for [y (range rows) x (range cols)] [x y])))))

(defn- render-hulls
  [^GL2 gl render-args node-id tile-set-attributes convex-hulls scale-factor collision-groups-data]
  (when (seq convex-hulls)
    (let [vbuf (gen-hulls-vbuf tile-set-attributes convex-hulls scale-factor collision-groups-data)
          vb (vtx/use-with node-id vbuf color-shader)]
      (gl/with-gl-bindings gl render-args [color-shader vb]
        (shader/set-uniform tile-shader gl "texture" 0)
        (gl/gl-draw-arrays gl GL2/GL_LINES 0 (count vbuf))))))

(defn render-tile-source
  [gl render-args renderables n]
  (let [{:keys [user-data]} (first renderables)
        {:keys [node-id tile-source-attributes uv-transforms gpu-texture convex-hulls collision-groups-data]} user-data
        scale-factor (camera/scale-factor (:camera render-args) (:viewport render-args))]
    (condp = (:pass render-args)
      pass/transparent
      (render-tiles gl render-args node-id gpu-texture tile-source-attributes uv-transforms scale-factor)

      pass/outline
      (do
        (render-tile-outlines gl render-args node-id tile-source-attributes convex-hulls scale-factor collision-groups-data)
        (render-hulls gl render-args node-id tile-source-attributes convex-hulls scale-factor collision-groups-data)))))

(g/defnk produce-scene
  [_node-id tile-source-attributes aabb uv-transforms texture-set gpu-texture convex-hulls collision-groups-data child-scenes]
  (when tile-source-attributes
    {:aabb aabb
     :renderable {:render-fn render-tile-source
                  :user-data {:node-id _node-id
                              :tile-source-attributes tile-source-attributes
                              :uv-transforms uv-transforms
                              :gpu-texture gpu-texture
                              :convex-hulls convex-hulls
                              :collision-groups-data collision-groups-data}
                  :passes [pass/transparent pass/outline]}
     :children child-scenes}))

(g/defnk produce-convex-hull-points
  [collision-resource original-convex-hulls tile-source-attributes]
  (if collision-resource
    (texture-set-gen/calculate-convex-hulls (image-util/read-image collision-resource) tile-source-attributes)
    original-convex-hulls))

(g/defnk produce-convex-hulls
  [convex-hull-points tile->collision-group-node collision-groups-data]
  (mapv (fn [points idx]
          (assoc points :collision-group (collision-groups/node->group collision-groups-data (tile->collision-group-node idx))))
        convex-hull-points
        (range)))

(g/defnk produce-tile->collision-group-node
  [tile->collision-group-node collision-groups-data]
  ;; remove tiles assigned collision group nodes that no longer exist
  (reduce-kv (fn [ret tile collision-group-node]
               (if-not (collision-groups/node->group collision-groups-data collision-group-node)
                 (dissoc ret tile)
                 ret))
             tile->collision-group-node
             tile->collision-group-node))

(g/defnk produce-tile-source-attributes
  [_node-id image-resource image-size tile-width tile-height tile-margin tile-spacing extrude-borders inner-padding collision-size]
  (or (validation/prop-error :fatal _node-id :image validation/prop-nil? image-resource "Image")
      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? image-resource "Image")
      (let [properties {:width tile-width
                        :height tile-height
                        :margin tile-margin
                        :spacing tile-spacing
                        :extrude-borders extrude-borders
                        :inner-padding inner-padding}
            metrics (texture-set-gen/calculate-tile-metrics image-size properties collision-size)]
        (when metrics
          (merge properties metrics)))
      (g/error-fatal "tile data could not be generated due to invalid values")))

(defn- check-anim-error [tile-count anim-data]
  (let [node-id (:node-id anim-data)
        anim (:ddf-message anim-data)
        tile-range-f (partial prop-tile-range? tile-count)]
    (->> [[:id validation/prop-empty?]
          [:start-tile tile-range-f]
          [:end-tile tile-range-f]]
      (keep (fn [[prop-kw f]]
              (validation/prop-error :fatal node-id prop-kw f (get anim prop-kw) (properties/keyword->name prop-kw)))))))

(g/defnk produce-texture-set-data
  [image image-resource tile-source-attributes animation-data collision-groups convex-hulls collision tile-count]
  (or (when-let [errors (not-empty (mapcat #(check-anim-error tile-count %) animation-data))]
        (g/error-aggregate errors))
      (let [animation-ddfs (mapv :ddf-message animation-data)]
        (texture-set-gen/tile-source->texture-set-data image-resource tile-source-attributes convex-hulls collision-groups animation-ddfs))))

(g/defnode TileSourceNode
  (inherits resource-node/ResourceNode)

  (property image resource/Resource
            (value (gu/passthrough image-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :image-resource]
                                            [:size :image-size])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (g/fnk [_node-id image tile-width-error tile-height-error image-dim-error]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? image "Image")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? image "Image")))))
  (property size types/Vec2
    (value (g/fnk [image-size]
             [(:width image-size 0) (:height image-size 0)]))
    (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
    (dynamic read-only? (g/constantly true)))

  (property tile-width g/Int
            (default 0)
            (dynamic error (g/fnk [_node-id tile-width tile-width-error]
                                  (validation/prop-error :fatal _node-id :tile-width validation/prop-negative? tile-width "Tile Width"))))

  (property tile-height g/Int
            (default 0)
            (dynamic error (g/fnk [_node-id tile-height tile-height-error]
                                  (validation/prop-error :fatal _node-id :tile-height validation/prop-negative? tile-height "Tile Height"))))

  (property tile-margin g/Int
            (default 0)
            (dynamic error (g/fnk [_node-id tile-margin tile-width-error tile-height-error]
                                  (validation/prop-error :fatal _node-id :tile-margin validation/prop-negative? tile-margin "Tile Margin"))))
  (property tile-spacing g/Int (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? tile-spacing)))
  (property extrude-borders g/Int (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? extrude-borders)))
  (property inner-padding g/Int (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? inner-padding)))
  (property collision resource/Resource ; optional
            (value (gu/passthrough collision-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :collision-resource]
                                            [:size :collision-size])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (g/fnk [_node-id collision image-dim-error tile-width-error tile-height-error]
                                  (validation/prop-error :fatal _node-id :collision validation/prop-resource-not-exists? collision "Collision"))))

  (property material-tag g/Str (default "tile") (dynamic visible (g/constantly false)))
  (property original-convex-hulls g/Any (dynamic visible (g/constantly false)))
  (property tile->collision-group-node g/Any (dynamic visible (g/constantly false)))

  (input build-settings g/Any)
  (input texture-profiles g/Any)
  (input collision-groups g/Any :array)
  (input animation-ddfs g/Any :array)
  (input animation-data g/Any :array)
  (input animation-ids g/Str :array)
  (input image-resource resource/Resource)
  (input image-size g/Any)
  (input collision-resource resource/Resource)
  (input collision-size g/Any)
  (input collision-groups-data g/Any)
  (input cleaned-convex-hulls g/Any :substitute [])
  (input child-scenes g/Any :array)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output tile-source-attributes g/Any :cached produce-tile-source-attributes)
  (output tile->collision-group-node g/Any :cached produce-tile->collision-group-node)

  (output texture-set-data g/Any :cached produce-texture-set-data)
  (output layout-size      g/Any               (g/fnk [texture-set-data] (:size texture-set-data)))
  (output texture-set      g/Any               (g/fnk [texture-set-data] (:texture-set texture-set-data)))
  (output uv-transforms    g/Any               (g/fnk [texture-set-data] (:uv-transforms texture-set-data)))
  (output packed-image     BufferedImage       (g/fnk [texture-set-data image-resource tile-source-attributes]
                                                 (texture-set-gen/layout-tile-source (:layout texture-set-data) (image-util/read-image image-resource) tile-source-attributes)))
  (output texture-image    g/Any               (g/fnk [packed-image texture-profile]
                                                 (tex-gen/make-preview-texture-image packed-image texture-profile)))

  (output convex-hull-points g/Any :cached produce-convex-hull-points)
  (output convex-hulls g/Any :cached produce-convex-hulls)

  (output aabb AABB :cached produce-aabb)
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-tile-source-outline)
  (output pb g/Any :cached produce-pb)
  (output save-value g/Any (g/fnk [pb] (dissoc pb :convex-hull-points)))


  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-texture g/Any :cached (g/fnk [_node-id texture-image]
                                      (texture/texture-image->gpu-texture _node-id
                                                                          texture-image
                                                                          {:min-filter gl/nearest
                                                                           :mag-filter gl/nearest})))
  (output anim-data g/Any :cached produce-anim-data)
  (output anim-ids g/Any :cached (gu/passthrough animation-ids))

  (output collision-groups-data g/Any :cached (gu/passthrough collision-groups-data))
  (output tile-count g/Int (g/fnk [tile-source-attributes]
                                       (* (:tiles-per-row tile-source-attributes) (:tiles-per-column tile-source-attributes))))
  (output image-dim-error g/Err (g/fnk [image-size collision-size]
                                       (when (and image-size collision-size)
                                         (let [{img-w :width img-h :height} image-size
                                               {coll-w :width coll-h :height} collision-size]
                                           (when (or (not= img-w coll-w)
                                                     (not= img-h coll-h))
                                             (g/error-fatal (format "both 'Image' and 'Collision' must have the same dimensions (%dx%d vs %dx%d)"
                                                                    img-w img-h
                                                                    coll-w coll-h)))))))
  (output tile-width-error g/Err (g/fnk [image-size collision-size tile-width tile-margin]
                                        (let [dims (or image-size collision-size)]
                                          (when dims
                                            (let [{w :width} dims
                                                  total-w (+ tile-width tile-margin)]
                                              (when (< w total-w)
                                                (g/error-fatal (format "the total width ('Tile Width' + 'Tile Margin') is greater than the 'Image' width (%d vs %d)"
                                                                       total-w w))))))))
  (output tile-height-error g/Err (g/fnk [image-size collision-size tile-height tile-margin]
                                         (let [dims (or image-size collision-size)]
                                           (when dims
                                             (let [{h :height} dims
                                                   total-h (+ tile-height tile-margin)]
                                               (when (< h total-h)
                                                 (g/error-fatal (format "the total height ('Tile Height' + 'Tile Margin') is greater than the 'Image' height (%d vs %d)"
                                                                        total-h h)))))))))


;;--------------------------------------------------------------------
;; tool

(defn world-pos->tile-pos
  [^Point3d pos tile-width tile-height x-border y-border]
  (let [x (long (Math/floor (/ (- (.x pos) x-border) (+ tile-width x-border))))
        y (long (Math/floor (/ (- (.y pos) y-border) (+ tile-height y-border))))]
    [x y]))

(g/defnk produce-active-tile
  [cursor-world-pos tile-source-attributes convex-hulls camera viewport]
  (when (and cursor-world-pos (seq convex-hulls))
    (let [{:keys [width height]} tile-source-attributes
          rows (:tiles-per-column tile-source-attributes)
          cols (:tiles-per-row tile-source-attributes)
          [scale-x scale-y] (camera/scale-factor camera viewport)
          x-border (* scale-x tile-border-size)
          y-border (* scale-y tile-border-size)
          [x y :as tile] (world-pos->tile-pos cursor-world-pos width height x-border y-border)
          convex-hull (nth convex-hulls (+ x (* (- rows y 1) cols)) nil)]
      (when (and (<= 0 x (dec cols))
                 (<= 0 y (dec rows))
                 (not (zero? (or (:count convex-hull) 0))))
        tile))))

(g/defnk produce-active-tile-idx
  [active-tile tile-source-attributes]
  (when active-tile
    (let [[x y] active-tile
          rows (:tiles-per-column tile-source-attributes)
          cols (:tiles-per-row tile-source-attributes)]
      (+ x (* (- rows y 1) cols)))))

(defn- render-tool
  [^GL2 gl render-args renderables n]
  (let [{:keys [user-data]} (first renderables)
        {:keys [node-id tile-source-attributes active-tile collision-groups-data selected-collision-group-node]} user-data
        {:keys [width height]} tile-source-attributes
        [scale-x scale-y] (camera/scale-factor (:camera render-args) (:viewport render-args))
        x-border (* scale-x tile-border-size)
        y-border (* scale-y tile-border-size)
        [x y] active-tile
        w width h height]
    (let [[r g b] (collision-groups/node->color collision-groups-data selected-collision-group-node)
          a (if (= pass/transparent (:pass render-args)) 0.30 1.0)
          vbuf (let [x0 (+ (* x (+ x-border w)) x-border)
                     x1 (+ x0 w)
                     y0 (+ (* y (+ y-border h)) y-border)
                     y1 (+ y0 h)]
                 (-> (->pos-color-vtx 4)
                     (conj! [x0 y0 0.0 r g b a])
                     (conj! [x0 y1 0.0 r g b a])
                     (conj! [x1 y1 0.0 r g b a])
                     (conj! [x1 y0 0.0 r g b a])
                     (persistent!)))
          vb (vtx/use-with node-id vbuf color-shader)]
      (gl/with-gl-bindings gl render-args [color-shader vb]
        (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf))))))

(g/defnk produce-tool-renderables
  [_node-id active-tile tile-source-attributes convex-hulls collision-groups-data selected-collision-group-node]
  (when active-tile
    (let [render-data [{:render-fn render-tool
                        :user-data {:node-id _node-id
                                    :active-tile active-tile
                                    :tile-source-attributes tile-source-attributes
                                    :convex-hulls convex-hulls
                                    :collision-groups-data collision-groups-data
                                    :selected-collision-group-node selected-collision-group-node}}]]
      {pass/outline render-data
       pass/transparent render-data})))

(defmulti begin-op (fn [op node action] op))
(defmulti update-op (fn [op node action] op))

(defmethod update-op nil
  [_ node action])

(defmethod begin-op :assign
  [_ node action]
  (when-let [active-tile-idx (g/node-value node :active-tile-idx)]
    (let [tile-source-node (g/node-value node :tile-source-node)
          collision-group-node (g/node-value node :selected-collision-group-node)
          op-seq (gensym)]
      [(g/operation-sequence op-seq)
       (g/set-property node :op-data {:op-seq op-seq
                                      :collision-group-node collision-group-node})
       (g/update-property tile-source-node :tile->collision-group-node assign-collision-group active-tile-idx collision-group-node)])))

(defmethod update-op :assign
  [_ node action]
  (when-let [active-tile-idx (g/node-value node :active-tile-idx)]
    (let [{:keys [op-seq collision-group-node]} (g/node-value node :op-data)
          tile-source-node (g/node-value node :tile-source-node)
          active-tile-idx (g/node-value node :active-tile-idx)]
      [(g/operation-sequence op-seq)
       (g/update-property tile-source-node :tile->collision-group-node assign-collision-group active-tile-idx collision-group-node)])))

(defn input-txs
  [self action tool-user-data]
  (let [op (g/node-value self :op)]
    (case (:type action)
      :mouse-pressed  (when-not (some? op)
                        (let [op :assign]
                          (when-let [op-txs (begin-op op self action)]
                            (concat
                             (g/set-property self :op op)
                             op-txs))))
      :mouse-moved    (concat
                       (g/set-property self :cursor-world-pos (:world-pos action))
                       (update-op op self action))
      :mouse-released (when (some? op)
                        (concat
                         (g/set-property self :op nil)
                         (g/set-property self :op-data nil)))

      nil)))

(defn handle-input
  [self action tool-user-data]
  (let [txs (input-txs self action tool-user-data)]
    (when (seq txs)
      (g/transact txs)
      true)))


(g/defnk produce-selected-collision-group-node
  [selected-node-ids]
  (->> selected-node-ids
       (filter #(g/node-instance? CollisionGroupNode %))
       single))

(g/defnode ToolController
  (property prefs g/Any)
  (property cursor-world-pos Point3d)
  (property op g/Keyword)
  (property op-data g/Any)

  ;; tool-controller contract
  (input active-tool g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  ;; additional inputs
  (input tile-source-node g/NodeID)
  (input selected-node-ids g/Any)
  (input tile-source-attributes g/Any)
  (input convex-hulls g/Any)
  (input collision-groups-data g/Any)

  (output active-tile g/Any :cached produce-active-tile)
  (output active-tile-idx g/Any :cached produce-active-tile-idx)
  (output selected-collision-group-node g/Any produce-selected-collision-group-node)
  (output renderables pass/RenderData :cached produce-tool-renderables)
  (output input-handler Runnable :cached (g/constantly handle-input)))

(defmethod scene/attach-tool-controller ::ToolController
  [_ tool-id view-id resource-id]
  (concat
   (g/connect view-id :selection tool-id :selected-node-ids)
   (g/connect resource-id :tile-source-attributes tool-id :tile-source-attributes)
   (g/connect resource-id :convex-hulls tool-id :convex-hulls)
   (g/connect resource-id :collision-groups-data tool-id :collision-groups-data)
   (g/connect resource-id :_node-id tool-id :tile-source-node)))

(defn- int->boolean [i]
  (not= 0 i))

(defn- make-animation-node [self project select-fn animation]
  (g/make-nodes
   (g/node-id->graph-id self)
   [animation-node [TileAnimationNode
                    :id (:id animation)
                    :start-tile (:start-tile animation)
                    :end-tile (:end-tile animation)
                    :playback (:playback animation)
                    :fps (:fps animation)
                    :flip-horizontal (int->boolean (:flip-horizontal animation))
                    :flip-vertical (int->boolean (:flip-vertical animation))
                    :cues (:cues animation)]]
   (attach-animation-node self animation-node)
   (when select-fn
     (select-fn [animation-node]))))

(defn- make-collision-group-node [self project select-fn collision-group]
  (g/make-nodes
   (g/node-id->graph-id self)
   [collision-group-node [CollisionGroupNode :id collision-group]]
   (attach-collision-group-node self collision-group-node)
   (when select-fn
     (select-fn [collision-group-node]))))

(defn- load-convex-hulls
  [{:keys [convex-hulls convex-hull-points]}]
  (if-not (= (count convex-hull-points) (* 2 (transduce (map :count) + convex-hulls)))
    []
    (mapv (fn [{:keys [index count]}]
            {:index index
             :count count
             :points (subvec convex-hull-points (* 2 index) (+ (* 2 index) (* 2 count)))})
          convex-hulls)))

(defn- make-tile->collision-group-node
  [{:keys [convex-hulls]} collision-group-nodes]
  (let [collision-group->node-id (into {} (keep (fn [tx]
                                                  (let [{:keys [_node-id id]} (:node tx)]
                                                    (when id [id _node-id])))
                                                collision-group-nodes))]
    (into {} (map-indexed (fn [idx {:keys [collision-group]}]
                            [idx (collision-group->node-id collision-group)])
                          convex-hulls))))

(defn- load-tile-source [project self resource tile-source]
  (let [image (workspace/resolve-resource resource (:image tile-source))
        collision (workspace/resolve-resource resource (:collision tile-source))
        collision-group-nodes (mapcat (partial make-collision-group-node self project nil) (set (:collision-groups tile-source)))
        animation-nodes (map (partial make-animation-node self project nil) (:animations tile-source))]
    (concat
      (g/connect project :build-settings self :build-settings)
      (g/connect project :texture-profiles self :texture-profiles)
      (for [field [:tile-width :tile-height :tile-margin :tile-spacing :material-tag :extrude-borders :inner-padding]]
        (g/set-property self field (field tile-source)))
      (g/set-property self :original-convex-hulls (load-convex-hulls tile-source))
      (g/set-property self :tile->collision-group-node (make-tile->collision-group-node tile-source collision-group-nodes))
      (g/set-property self :image image)
      (g/set-property self :collision collision)
      (g/connect self :convex-hulls self :cleaned-convex-hulls)
      animation-nodes
      collision-group-nodes
      (g/connect project :collision-groups-data self :collision-groups-data))))

(def ^:private default-animation
  {:id "New Animation"
   :start-tile 1
   :end-tile 1
   :playback :playback-once-forward
   :fps 30
   :flip-horizontal 0
   :flip-vertical 0 ; yes, wierd integer booleans
   :cues '()})

(defn add-animation-node! [self select-fn]
  (g/transact (make-animation-node self (project/get-project self) select-fn default-animation)))

(defn- gen-unique-name
  [basename existing-names]
  (let [existing-names (set existing-names)]
    (loop [postfix 0]
      (let [name (if (= postfix 0) basename (str basename postfix))]
        (if (existing-names name)
          (recur (inc postfix))
          name)))))

(defn add-collision-group-node!
  [self select-fn]
  (let [project (project/get-project self)
        collision-groups-data (g/node-value project :collision-groups-data)
        collision-group (gen-unique-name "New Collision Group" (collision-groups/collision-groups collision-groups-data))]
    (g/transact
      (make-collision-group-node self project select-fn collision-group))))

(defn- selection->tile-source [selection]
  (handler/adapt-single selection TileSourceNode))

(handler/defhandler :add :workbench
  (active? [selection] (selection->tile-source selection))
  (label [selection user-data]
         (if-not user-data
           "Add"
           (:label user-data)))
  (options [selection user-data]
           (when-not user-data
             [{:label "Animation"
               :icon animation-icon
               :command :add
               :user-data {:action add-animation-node!}}
              {:label "Collision Group"
               :icon collision-icon
               :command :add
               :user-data {:action add-collision-group-node!}}
              ]))
  (run [selection user-data app-view]
    ((:action user-data) (selection->tile-source selection) (fn [node-ids] (app-view/select app-view node-ids)))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext ["tilesource" "tileset"]
                                    :label "Tile Source"
                                    :build-ext "texturesetc"
                                    :node-type TileSourceNode
                                    :ddf-type Tile$TileSet
                                    :load-fn load-tile-source
                                    :icon tile-source-icon
                                    :view-types [:scene :test]
                                    :view-opts {:scene {:tool-controller ToolController}}))
