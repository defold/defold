(ns editor.tile-source
  (:require
   [clojure.string :as str]
   [dynamo.graph :as g]
   [editor.camera :as camera]
   [editor.colors :as colors]
   [editor.collision-groups :as collision-groups]
   [editor.graph-util :as gu]
   [editor.workspace :as workspace]
   [editor.resource :as resource]
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
   [editor.scene :as scene]
   [editor.outline :as outline]
   [editor.protobuf :as protobuf]
   [editor.util :as util]
   [editor.validation :as validation])
  (:import
   [com.dynamo.tile.proto Tile$TileSet Tile$Playback]
   [com.dynamo.textureset.proto TextureSetProto$TextureSet]
   [com.google.protobuf ByteString]
   [editor.types AABB]
   [javax.vecmath Point3d Matrix4d]
   [javax.media.opengl GL GL2]
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

;; TODO - fix real profiles
(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(defn assign-collision-group
  [tile->collision-group tile group-node-id]
  (assoc tile->collision-group tile group-node-id))

(g/defnk produce-pb
  ;; this will be called both
  ;; * during save-data production with ignore-errors <severe> meaning f.i. image may be unvalidated (nil)
  ;; * during produce-build-targets with normal validation
  [image tile-width tile-height tile-margin tile-spacing collision material-tag
   convex-hulls collision-groups animation-ddfs extrude-borders inner-padding]
  {:image (resource/resource->proj-path image)
   :tile-width tile-width
   :tile-height tile-height
   :tile-margin tile-margin
   :tile-spacing tile-spacing
   :collision (when collision (resource/proj-path collision))
   :material-tag material-tag
   :convex-hulls (mapv (fn [{:keys [index count collision-group]}]
                         {:index index
                          :count count
                          :collision-group (or collision-group "")}) convex-hulls)
   :convex-hull-points (vec (mapcat :points convex-hulls))
   :collision-groups (sort collision-groups)
   :animations (sort-by :id animation-ddfs)
   :extrude-borders extrude-borders
   :inner-padding inner-padding})

(g/defnk produce-save-data [resource pb]
  {:resource resource
   :content (protobuf/map->str Tile$TileSet pb)})

(defn- build-texture [self basis resource dep-resources user-data]
  {:resource resource :content (tex-gen/->bytes (:image user-data) test-profile)})

(defn- build-texture-set [self basis resource dep-resources user-data]
  (let [tex-set (assoc (:proto user-data) :texture (resource/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes TextureSetProto$TextureSet tex-set)}))

(g/defnk produce-build-targets [_node-id resource texture-set-data save-data]
  (let [workspace        (project/workspace (project/get-project _node-id))
        texture-type     (workspace/get-resource-type workspace "texture")
        texture-resource (resource/make-memory-resource workspace texture-type (:content save-data))
        texture-target   {:node-id   _node-id
                          :resource  (workspace/make-build-resource texture-resource)
                          :build-fn  build-texture
                          :user-data {:image (:image texture-set-data)}}]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture-set
      :user-data {:proto (:texture-set texture-set-data)}
      :deps [texture-target]}]))



;; TODO - copied from atlas, generalize into separate file - texture-set-util?

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

(g/defnode CollisionGroupNode
  (inherits outline/OutlineNode)

  (property id g/Str
            ;; can't do this b/c cycle
            #_(validate (g/fnk [collision-groups-data]
                          (when (collision-groups/overallocated? collision-groups-data)
                            (g/error-warning "More than 16 collision groups in use.")))))

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

(g/defnode TileAnimationNode
  (inherits outline/OutlineNode)
  (property id g/Str)
  (property start-tile g/Int)
  (property end-tile g/Int)
  (property playback types/AnimationPlayback
            (default :playback-once-forward)
            (dynamic edit-type (g/always
                                (let [options (protobuf/enum-values Tile$Playback)]
                                  {:type :choicebox
                                   :options (zipmap (map first options)
                                                    (map (comp :display-name second) options))}))))
  (property fps g/Int (default 30))
  (property flip-horizontal g/Bool (default false))
  (property flip-vertical g/Bool (default false))
  (property cues g/Any (dynamic visible (g/always false)))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id id] {:node-id _node-id :label id :icon animation-icon}))
  (output ddf-message g/Any produce-animation-ddf))

(defn- attach-animation-node [self animation-node]
  (for [[from to] [[:_node-id :nodes]
                   [:node-outline :child-outlines]
                   [:ddf-message :animation-ddfs]
                   [:id :animation-ids]]]
    (g/connect animation-node from self to)))

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
  {:node-id _node-id
   :label "Tile Source"
   :icon tile-source-icon
   :children (vec (sort-by outline-sort-by-fn child-outlines))
   :child-reqs [{:node-type TileAnimationNode
                 :tx-attach-fn attach-animation-node}
                {:node-type CollisionGroupNode
                 :tx-attach-fn attach-collision-group-node}]})

(g/defnk produce-aabb
  [tile-source-attributes]
  (if tile-source-attributes
    (let [{:keys [visual-width visual-height]} tile-source-attributes]
      (types/->AABB (Point3d. 0 0 0) (Point3d. visual-width visual-height 0)))
    (geom/null-aabb)))

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

(defn gen-tiles-vbuf
  [tile-source-attributes texture-set-data [scale-x scale-y]]
  (let [uvs (:uv-transforms texture-set-data)
        w (:width tile-source-attributes)
        h (:height tile-source-attributes)
        rows (:tiles-per-column tile-source-attributes)
        cols (:tiles-per-row tile-source-attributes)
        x-border (* scale-x tile-border-size)
        y-border (* scale-y tile-border-size)]
    (persistent!
     (reduce (fn [vbuf [x y]]
               (let [uv (nth uvs (+ x (* y cols)))
                     x0 (+ (* x (+ x-border w)) x-border)
                     x1 (+ x0 w)
                     y0 (+ (* (- rows y 1) (+ y-border h)) y-border)
                     y1 (+ y0 h)
                     [[u0 v0] [u1 v1]] (geom/uv-trans uv [[0 0] [1 1]])]
                 (-> vbuf
                     (conj! [x0 y0 0 1 u0 v1])
                     (conj! [x0 y1 0 1 u0 v0])
                     (conj! [x1 y1 0 1 u1 v0])
                     (conj! [x1 y0 0 1 u1 v1]))))
             (->pos-uv-vtx (* 4 rows cols))
             (for [y (range rows) x (range cols)] [x y])))))

(defn- render-tiles
  [^GL2 gl render-args node-id gpu-texture tile-source-attributes texture-set-data scale-factor]
  (def nid node-id)
  (let [vbuf (gen-tiles-vbuf tile-source-attributes texture-set-data scale-factor)
        vb (vtx/use-with node-id vbuf tile-shader)]
    (gl/with-gl-bindings gl render-args [gpu-texture tile-shader vb]
      (shader/set-uniform tile-shader gl "texture" 0)
      (gl/gl-draw-arrays gl GL2/GL_QUADS 0 (count vbuf)))))

(defn gen-tile-outlines-vbuf
  [tile-set-attributes convex-hulls [scale-x scale-y] collision-groups-data]
  (let [w (:width tile-set-attributes)
        h (:height tile-set-attributes)
        rows (:tiles-per-column tile-set-attributes)
        cols (:tiles-per-row tile-set-attributes)
        x-border (* scale-x tile-border-size)
        y-border (* scale-y tile-border-size)]
    (persistent!
     (reduce (fn [vbuf [x y]]
               (let [x0 (+ (* x (+ x-border w)) x-border)
                     x1 (+ x0 w)
                     y0 (+ (* (- rows y 1) (+ y-border h)) y-border)
                     y1 (+ y0 h)
                     {:keys [points collision-group]} (nth convex-hulls (+ x (* y cols)) nil)
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
             (for [y (range rows) x (range cols)] [x y])))))

(defn- render-tile-outlines
  [^GL2 gl render-args node-id tile-set-attributes convex-hulls scale-factor collision-groups-data]
  (let [vbuf (gen-tile-outlines-vbuf tile-set-attributes convex-hulls scale-factor collision-groups-data)
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
        {:keys [node-id tile-source-attributes texture-set-data gpu-texture convex-hulls collision-groups-data]} user-data
        scale-factor (camera/scale-factor (:camera render-args) (:viewport render-args))]
    (condp = (:pass render-args)
      pass/transparent
      (render-tiles gl render-args node-id gpu-texture tile-source-attributes texture-set-data scale-factor)

      pass/outline
      (do
        (render-tile-outlines gl render-args node-id tile-source-attributes convex-hulls scale-factor collision-groups-data)
        (render-hulls gl render-args node-id tile-source-attributes convex-hulls scale-factor collision-groups-data)))))

(g/defnk produce-scene
  [_node-id tile-source-attributes aabb texture-set-data gpu-texture convex-hulls collision-groups-data]
  (when tile-source-attributes
    {:aabb aabb
     :renderable {:render-fn render-tile-source
                  :user-data {:node-id _node-id
                              :tile-source-attributes tile-source-attributes
                              :texture-set-data texture-set-data
                              :gpu-texture gpu-texture
                              :convex-hulls convex-hulls
                              :collision-groups-data collision-groups-data}
                  :passes [pass/transparent pass/outline]}}))

(g/defnk validate-tile-width [tile-width tile-margin ^BufferedImage image-content ^BufferedImage collision-content]
  (if-let [max-width (or (and image-content (.getWidth image-content))
                         (and collision-content (.getWidth collision-content)))]
    (let [total-width (+ tile-width tile-margin)]
      (when (> total-width max-width)
        (g/error-severe (format "The total tile width (including margin) %d is greater than the image width %d" total-width max-width))))))

(g/defnk validate-tile-height [tile-height tile-margin ^BufferedImage image-content ^BufferedImage collision-content]
  (if-let [max-height (or (and image-content (.getHeight image-content))
                          (and collision-content (.getHeight collision-content)))]
    (let [total-height (+ tile-height tile-margin)]
      (when (> total-height max-height)
        (g/error-severe (format "The total tile height (including margin) %d is greater than the image height %d" total-height max-height))))))

(g/defnk validate-tile-dimensions [tile-width tile-height tile-margin image-content collision-content :as all]
  (validation/error-aggregate [(validate-tile-width all) (validate-tile-height all)]))


(g/defnk produce-convex-hull-points
  [collision-content original-convex-hulls tile-source-attributes]
  (if collision-content
    (texture-set-gen/calculate-convex-hulls collision-content tile-source-attributes)
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
  [_node-id image-content tile-width tile-height tile-margin tile-spacing extrude-borders inner-padding collision-content]
  (def nid _node-id)
  (let [properties {:width tile-width
                    :height tile-height
                    :margin tile-margin
                    :spacing tile-spacing
                    :extrude-borders extrude-borders
                    :inner-padding inner-padding}
        metrics (texture-set-gen/calculate-tile-metrics image-content properties collision-content)]
    (when metrics
      (merge properties metrics))))

(g/defnk produce-texture-set-data
  [image image-content tile-source-attributes animation-ddfs collision-groups convex-hulls collision collision-content]
  (texture-set-gen/tile-source->texture-set-data image-content tile-source-attributes convex-hulls collision-groups animation-ddfs))

(g/defnode TileSourceNode
  (inherits project/ResourceNode)

  (property image resource/Resource
            (value (gu/passthrough image-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :image-resource]
                                            [:content :image-content])))
            (validate (validation/validate-resource image-resource "Missing image"
                                                    [image-content])))

  (property tile-width g/Int
            (default 0)
            (validate validate-tile-width))

  (property tile-height g/Int
            (default 0)
            (validate validate-tile-height))

  (property tile-margin g/Int
            (default 0)
            (validate validate-tile-dimensions))

  (property tile-spacing g/Int (default 0))

  (property collision resource/Resource ; optional
            (value (gu/passthrough collision-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :collision-resource]
                                            [:content :collision-content])))
            (validate (g/fnk [collision-resource collision-content]
                        ;; collision-resource is optional, but depend on resource & content to trigg auto error aggregation
                        )))

  (property material-tag g/Str (default "tile"))
  (property extrude-borders g/Int (default 0))
  (property inner-padding g/Int (default 0))
  (property original-convex-hulls g/Any (dynamic visible (g/always false)))
  (property tile->collision-group-node g/Any (dynamic visible (g/always false)))

  (input collision-groups g/Any :array)
  (input animation-ddfs g/Any :array)
  (input animation-ids g/Str :array)
  (input image-resource resource/Resource)
  (input image-content BufferedImage)
  (input collision-resource resource/Resource)
  (input collision-content BufferedImage)
  (input collision-groups-data g/Any)

  (output tile-source-attributes g/Any :cached produce-tile-source-attributes)
  (output tile->collision-group-node g/Any :cached produce-tile->collision-group-node)
  (output texture-set-data g/Any :cached produce-texture-set-data)
  (output convex-hull-points g/Any :cached produce-convex-hull-points)
  (output convex-hulls g/Any :cached produce-convex-hulls)

  (output aabb AABB :cached produce-aabb)
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-tile-source-outline)
  (output pb g/Any :cached produce-pb)
  (output save-data g/Any :cached produce-save-data)

  (output packed-image BufferedImage (g/fnk [texture-set-data] (:image texture-set-data)))
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-texture g/Any :cached (g/fnk [_node-id texture-set-data] (texture/image-texture _node-id (:image texture-set-data))))
  (output anim-data g/Any :cached produce-anim-data)
  (output anim-ids g/Any :cached (gu/passthrough animation-ids))

  (output collision-groups-data g/Any :cached (gu/passthrough collision-groups-data)))


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
  (property cursor-world-pos Point3d)
  (property tile-source-node g/NodeID)
  (property op g/Keyword)
  (property op-data g/Any)

  ;; tool-controller contract
  (input active-tool g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  ;; additional inputs
  (input selected-node-ids g/Any)
  (input tile-source-attributes g/Any)
  (input convex-hulls g/Any)
  (input collision-groups-data g/Any)

  (output active-tile g/Any :cached produce-active-tile)
  (output active-tile-idx g/Any :cached produce-active-tile-idx)
  (output selected-collision-group-node g/Any produce-selected-collision-group-node)
  (output renderables pass/RenderData :cached produce-tool-renderables)
  (output input-handler Runnable :cached (g/always handle-input)))

(defmethod scene/attach-tool-controller ::ToolController
  [_ tool-id view-id resource-id]
  (concat
   (g/connect view-id :selection tool-id :selected-node-ids)
   (g/connect resource-id :tile-source-attributes tool-id :tile-source-attributes)
   (g/connect resource-id :convex-hulls tool-id :convex-hulls)
   (g/connect resource-id :collision-groups-data tool-id :collision-groups-data)
   (g/set-property tool-id :tile-source-node resource-id)))

(defn- int->boolean [i]
  (not= 0 i))

(defn- make-animation-node [self animation]
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
   (attach-animation-node self animation-node)))

(defn- make-collision-group-node [self collision-group]
  (g/make-nodes
   (g/node-id->graph-id self)
   [collision-group-node [CollisionGroupNode :id collision-group]]
   (attach-collision-group-node self collision-group-node)))

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
  (let [collision-group->node-id (into {} (keep (fn [{:keys [_node-id id]}] (when id [_node-id id])) collision-group-nodes))]
    (into {} (map-indexed (fn [idx {:keys [collision-group]}]
                            [idx (collision-group->node-id collision-group)])
                          convex-hulls))))

(defn- load-tile-source
  [project self resource]
  (let [tile-source (protobuf/read-text Tile$TileSet resource)
        image (workspace/resolve-resource resource (:image tile-source))
        collision (workspace/resolve-resource resource (:collision tile-source))
        collision-group-nodes (map (partial make-collision-group-node self) (set (:collision-groups tile-source)))
        animation-nodes (map (partial make-animation-node self) (:animations tile-source))]
    (concat
     (for [field [:tile-width :tile-height :tile-margin :tile-spacing :material-tag :extrude-borders :inner-padding]]
       (g/set-property self field (field tile-source)))
     (g/set-property self :original-convex-hulls (load-convex-hulls tile-source))
     (g/set-property self :tile->collision-group-node (make-tile->collision-group-node tile-source collision-group-nodes))
     (g/set-property self :image image)
     (g/set-property self :collision collision)
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

(defn- add-animation-node [self]
  (g/transact (make-animation-node self default-animation)))

(defn- gen-unique-name
  [basename existing-names]
  (let [existing-names (set existing-names)]
    (loop [postfix 0]
      (let [name (if (= postfix 0) basename (str basename postfix))]
        (if (existing-names name)
          (recur (inc postfix))
          name)))))

(defn- add-collision-group-node
  [self]
  (let [project (project/get-project self)
        collision-groups-data (g/node-value project :collision-groups-data)
        collision-group (gen-unique-name "New Collision Group" (collision-groups/collision-groups collision-groups-data))]
    (g/transact (make-collision-group-node self collision-group))))

(handler/defhandler :add :global
  (active? [selection] (handler/single-selection? selection TileSourceNode))
  (label [selection user-data]
         (if-not user-data
           "Add"
           (:label user-data)))
  (options [selection user-data]
           (when-not user-data
             [{:label "Animation"
               :icon animation-icon
               :command :add
               :user-data {:action add-animation-node}}
              {:label "Collision Group"
               :icon collision-icon
               :command :add
               :user-data {:action add-collision-group-node}}
              ]))
  (run [selection user-data]
    ((:action user-data) (first selection))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext ["tilesource" "tileset"]
                                    :label "Tile Source"
                                    :build-ext "texturesetc"
                                    :node-type TileSourceNode
                                    :load-fn load-tile-source
                                    :icon tile-source-icon
                                    :view-types [:scene :test]
                                    :view-opts {:scene {:tool-controller ToolController}}))
