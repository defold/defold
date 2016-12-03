(ns editor.atlas
  (:require [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.image :as image]
            [editor.geom :as geom]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.texture :as tex]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.pipeline :as pipeline]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.scene :as scene]
            [editor.outline :as outline]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.image :as image])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
           [com.dynamo.tile.proto Tile$Playback]
           [com.jogamp.opengl.util.awt TextRenderer]
           [com.google.protobuf ByteString]
           [editor.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]
           [java.nio ByteBuffer ByteOrder FloatBuffer]))

(set! *warn-on-reflection* true)

(defn mind ^double [^double a ^double b] (Math/min a b))
(defn maxd ^double [^double a ^double b] (Math/max a b))

(def atlas-icon "icons/32/Icons_13-Atlas.png")
(def animation-icon "icons/32/Icons_24-AT-Animation.png")
(def image-icon "icons/32/Icons_25-AT-Image.png")

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
  [gl render-args vertex-binding gpu-texture]
  (gl/with-gl-bindings gl render-args [gpu-texture atlas-shader vertex-binding]
    (shader/set-uniform atlas-shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)))

(defn render-image-outline
  [^GL2 gl render-args renderable image-data]
  (let [rect (:rect image-data)
        x0 (:x rect)
        y0 (:y rect)
        x1 (+ x0 (:width rect))
        y1 (+ y0 (:height rect))
        [cr cg cb ca] scene/selected-outline-color]
    (.glColor4d gl cr cg cb ca)
    (.glBegin gl GL2/GL_QUADS)
    (.glVertex3d gl x0 y0 0)
    (.glVertex3d gl x0 y1 0)
    (.glVertex3d gl x1 y1 0)
    (.glVertex3d gl x1 y0 0)
    (.glEnd gl)))

(defn- path->id [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn image->animation [image id]
  (types/map->Animation {:id              id
                         :images          [image]
                         :fps             30
                         :flip-horizontal false
                         :flip-vertical   false
                         :playback        :playback-once-forward}))

(g/defnk produce-image-scene
  [_node-id id image-data]
  (let [image-data (get image-data id)]
    {:node-id _node-id
     :aabb (geom/rect->aabb (:rect image-data))
     :renderable {:render-fn (fn [gl render-args [renderable] n]
                               (condp = (:pass render-args)
                                 pass/outline
                                 (when (:selected renderable)
                                   (render-image-outline gl render-args renderable image-data))

                                 pass/selection
                                 (render-image-outline gl render-args renderable image-data)))
                  :passes [pass/outline pass/selection]}}))

(g/defnode AtlasImage
  (inherits outline/OutlineNode)

  (property size types/Vec2
    (value (g/fnk [^BufferedImage src-image]
             (if src-image
               [(.getWidth src-image) (.getHeight src-image)]
               [0 0])))
    (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
    (dynamic read-only? (g/constantly true)))
  (property order g/Int (dynamic visible (g/constantly false)) (default 0))

  (input src-resource resource/Resource)
  (input src-image BufferedImage)
  (input image-data g/Any)

  (output image-order g/Any (g/fnk [_node-id order] [_node-id order]))
  (output path g/Str (g/fnk [src-resource] (resource/proj-path src-resource)))
  (output id g/Str (g/fnk [path] (path->id path)))
  (output image Image (g/fnk [_node-id path ^BufferedImage src-image]
                             (Image. path src-image (.getWidth src-image) (.getHeight src-image))))
  (output animation Animation (g/fnk [image id] (image->animation image id)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id id src-resource order]
                                                          (cond-> {:node-id _node-id
                                                                   :label id
                                                                   :order order
                                                                   :icon image-icon}
                                                            src-resource (assoc :link src-resource))))
  (output ddf-message g/Any :cached (g/fnk [path order] {:image path :order order}))
  (output scene g/Any :cached produce-image-scene))

(defn- sort-by-and-strip-order [images]
  (->> images
       (sort-by :order)
       (map #(dissoc % :order))))

(g/defnk produce-anim-ddf [id fps flip-horizontal flip-vertical playback img-ddf]
  {:id id
   :fps fps
   :flip-horizontal flip-horizontal
   :flip-vertical flip-vertical
   :playback playback
   :images (sort-by-and-strip-order img-ddf)})

(defn- attach-image [parent labels image-node scope-node image-order]
  (concat
    (g/set-property image-node :order image-order)
    (g/connect image-node :image-order  parent     :image-order)
    (g/connect image-node :_node-id     scope-node :nodes)
    (for [[from to] labels]
      (g/connect image-node from parent to))
    (g/connect image-node :node-outline parent     :child-outlines)
    (g/connect image-node :ddf-message  parent     :img-ddf)
    (g/connect image-node :scene        parent     :child-scenes)
    (g/connect parent     :image-data   image-node :image-data)))

(defn- attach-animation [atlas-node animation-node]
  (concat
   (g/connect animation-node :_node-id     atlas-node :nodes)
   (g/connect animation-node :animation    atlas-node :animations)
   (g/connect animation-node :id           atlas-node :animation-ids)
   (g/connect animation-node :node-outline atlas-node :child-outlines)
   (g/connect animation-node :ddf-message  atlas-node :anim-ddf)
   (g/connect animation-node :scene        atlas-node :child-scenes)
   (g/connect atlas-node     :image-data   animation-node :image-data)))

(defn- next-image-order [parent]
  (inc (apply max -1 (map second (g/node-value parent :image-order)))))

(defn- tx-attach-image-to-animation [animation-node image-node]
  (attach-image animation-node
                [[:image :frames]]
                image-node
                (core/scope animation-node)
                (next-image-order animation-node)))

(g/defnk produce-animation-scene
  [_node-id child-scenes]
  {:node-id _node-id
   :aabb (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes))
   :children child-scenes})

(g/defnode AtlasAnimation
  (inherits outline/OutlineNode)

  (property id g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? id)))
  (property fps g/Int
            (default 30)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? fps)))
  (property flip-horizontal g/Bool)
  (property flip-vertical   g/Bool)
  (property playback        types/AnimationPlayback
            (dynamic edit-type (g/constantly
                                 (let [options (protobuf/enum-values Tile$Playback)]
                                   {:type :choicebox
                                    :options (zipmap (map first options)
                                                     (map (comp :display-name second) options))}))))

  (input frames Image :array)
  (input img-ddf g/Any :array)
  (input child-scenes g/Any :array)
  (input image-order g/Any :array)
  (input image-data g/Any)

  (output animation Animation (g/fnk [this id frames fps flip-horizontal flip-vertical playback]
                                (types/->Animation id frames fps flip-horizontal flip-vertical playback)))
  (output image-data g/Any (gu/passthrough image-data))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id child-outlines] {:node-id _node-id
                                         :label id
                                         :children (sort-by :order child-outlines)
                                         :icon animation-icon
                                         :child-reqs [{:node-type AtlasImage
                                                       :tx-attach-fn tx-attach-image-to-animation}]}))
  (output ddf-message g/Any :cached produce-anim-ddf)
  (output scene g/Any :cached produce-animation-scene))

(g/defnk produce-save-data [resource margin inner-padding extrude-borders img-ddf anim-ddf]
  {:resource resource
   :content (let [m {:margin margin
                     :inner-padding inner-padding
                     :extrude-borders extrude-borders
                     :images (sort-by-and-strip-order img-ddf)
                     :animations anim-ddf}]
              (protobuf/map->str AtlasProto$Atlas m))})

; TODO - fix real profiles
(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(g/defnk produce-build-targets [_node-id resource texture-set-data save-data]
  (let [project           (project/get-project _node-id)
        workspace         (project/workspace project)
        texture-target    (image/make-texture-build-target workspace _node-id (:image texture-set-data))
        pb-msg            (:texture-set texture-set-data)
        dep-build-targets [texture-target]]
    [(pipeline/make-protobuf-build-target _node-id resource dep-build-targets
                                          TextureSetProto$TextureSet
                                          (assoc pb-msg :texture (-> texture-target :resource :resource))
                                          [:texture])]))

(defn gen-renderable-vertex-buffer
  [width height]
  (let [x0 0
        y0 0
        x1 width
        y1 height]
    (persistent!
      (doto (->texture-vtx 6)
           (conj! [x0 y0 0 1 0 0])
           (conj! [x0 y1 0 1 0 1])
           (conj! [x1 y1 0 1 1 1])

           (conj! [x1 y1 0 1 1 1])
           (conj! [x1 y0 0 1 1 0])
           (conj! [x0 y0 0 1 0 0])))))

(g/defnk produce-scene
  [_node-id texture-set-data aabb gpu-texture child-scenes]
  (let [^BufferedImage img (:image texture-set-data)
        width (.getWidth img)
        height (.getHeight img)
        vertex-buffer (gen-renderable-vertex-buffer width height)
        vertex-binding (vtx/use-with _node-id vertex-buffer atlas-shader)]
    {:aabb aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (render-texture-set gl render-args vertex-binding gpu-texture))
                  :passes [pass/transparent]}
     :children child-scenes}))

(g/defnk produce-texture-set-data [_node-id animations images margin inner-padding extrude-borders]
  (or (when-let [errors (->> [[margin "Margin"]
                              [inner-padding "Inner Padding"]
                              [extrude-borders "Extrude Borders"]]
                          (keep (fn [[v name]]
                                  (validation/prop-error :fatal _node-id :texture-set-data validation/prop-negative? v name)))
                          not-empty)]
        (g/error-aggregate errors))
      (texture-set-gen/->texture-set-data animations images margin inner-padding extrude-borders)))

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


(defn- tex-coords->rect
  [^FloatBuffer tex-coords index atlas-width atlas-height]
  (let [quad (->uv-quad index tex-coords)
        x0 (reduce mind (map first quad))
        y0 (reduce mind (map second quad))
        x1 (reduce maxd (map first quad))
        y1 (reduce maxd (map second quad))
        w (- x1 x0)
        h (- y1 y0)]
    (types/rect (* x0 atlas-width)
                (* y0 atlas-height)
                (* w atlas-width)
                (* h atlas-height))))

(defn- ->image-data
  [index tex-coords atlas-width atlas-height]
  {:rect (tex-coords->rect tex-coords index atlas-width atlas-height)})

(g/defnk produce-image-data [images texture-set-data]
  (let [tex-set (:texture-set texture-set-data)
        image ^BufferedImage (:image texture-set-data)
        width (.getWidth image)
        height (.getHeight image)
        tex-coords (-> ^ByteString (:tex-coords tex-set)
                       (.asReadOnlyByteBuffer)
                       (.order ByteOrder/LITTLE_ENDIAN)
                       (.asFloatBuffer))
        xform (map-indexed (fn [idx image]
                             [(path->id (:path image)) (->image-data idx tex-coords width height)]))]
    (into {} xform images)))

(defn- tx-attach-image-to-atlas [atlas-node image-node]
  (attach-image atlas-node
                [[:animation :animations]
                 [:id :animation-ids]]
                image-node
                atlas-node
                (next-image-order atlas-node)))

(defn- atlas-outline-sort-by-fn [v]
  [(:name (g/node-type* (:node-id v)))])

(g/defnode AtlasNode
  (inherits project/ResourceNode)

  (property size types/Vec2
    (value (g/fnk [texture-set-data]
             (if-let [^BufferedImage img (:image texture-set-data)]
               [(.getWidth img) (.getHeight img)]
               [0 0])))
    (dynamic edit-type (g/constantly {:type types/Vec2 :labels ["W" "H"]}))
    (dynamic read-only? (g/constantly true)))
  (property margin g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? margin)))
  (property inner-padding g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? inner-padding)))
  (property extrude-borders g/Int
            (default 0)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? extrude-borders)))

  (input animations Animation :array)
  (input animation-ids g/Str :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)
  (input image-order g/Any :array)
  (input child-scenes g/Any :array)

  (output images           [Image]             :cached (g/fnk [animations] (vals (into {} (map (fn [img] [(:path img) img]) (mapcat :images animations))))))
  (output aabb             AABB                (g/fnk [texture-set-data] (let [^BufferedImage img (:image texture-set-data)] (types/->AABB (Point3d. 0 0 0) (Point3d. (.getWidth img) (.getHeight img) 0)))))
  (output gpu-texture      g/Any               :cached (g/fnk [_node-id texture-set-data] (texture/image-texture _node-id (:image texture-set-data))))
  (output texture-set-data g/Any               :cached produce-texture-set-data)
  (output packed-image     BufferedImage       (g/fnk [texture-set-data] (:image texture-set-data)))
  (output anim-data        g/Any               :cached produce-anim-data)
  (output image-data       g/Any               :cached produce-image-data)
  (output anim-ids         g/Any               :cached (gu/passthrough animation-ids))
  (output node-outline     outline/OutlineData :cached (g/fnk [_node-id child-outlines] {:node-id _node-id
                                                                                         :label "Atlas"
                                                                                         :children (->> child-outlines
                                                                                                     (sort-by atlas-outline-sort-by-fn)
                                                                                                     vec)
                                                                                         :icon atlas-icon
                                                                                         :child-reqs [{:node-type AtlasImage
                                                                                                       :tx-attach-fn tx-attach-image-to-atlas
                                                                                                       }
                                                                                                      {:node-type AtlasAnimation
                                                                                                       :tx-attach-fn attach-animation
                                                                                                       }
                                                                                                      ]
                                                                                         }))
  (output save-data        g/Any          :cached produce-save-data)
  (output build-targets    g/Any          :cached produce-build-targets)
  (output scene            g/Any          :cached produce-scene))

(def ^:private atlas-animation-keys [:flip-horizontal :flip-vertical :fps :playback :id])

(defn- attach-image-nodes
  [parent labels images scope-node]
  (let [graph-id (g/node-id->graph-id parent)]
    (let [next-order (next-image-order parent)]
      (map-indexed
       (fn [index image]
         (let [image-order (+ next-order index)]
           (g/make-nodes
            graph-id
            [atlas-image [AtlasImage]]
            (project/connect-resource-node (project/get-project scope-node) image atlas-image [[:content :src-image]
                                                                                              [:resource :src-resource]])
            (attach-image parent labels atlas-image scope-node image-order))))
       images))))


(defn add-images [atlas-node img-resources]
  ; used by tests
  (attach-image-nodes atlas-node
                      [[:animation :animations]
                       [:id :animation-ids]]
                      img-resources atlas-node))

(defn- attach-atlas-animation [atlas-node anim]
  (let [graph-id (g/node-id->graph-id atlas-node)]
    (g/make-nodes
     graph-id
     [atlas-anim [AtlasAnimation :flip-horizontal (not= 0 (:flip-horizontal anim)) :flip-vertical (not= 0 (:flip-vertical anim))
                  :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
     (attach-animation atlas-node atlas-anim)
     (attach-image-nodes atlas-anim [[:image :frames]] (map :image (:images anim)) atlas-node))))

(defn load-atlas [project self resources]
  (let [atlas         (protobuf/read-text AtlasProto$Atlas resources)
        graph-id      (g/node-id->graph-id self)]
    (concat
      (g/set-property self :margin (:margin atlas))
      (g/set-property self :inner-padding (:inner-padding atlas))
      (g/set-property self :extrude-borders (:extrude-borders atlas))
      (attach-image-nodes self
                          [[:animation :animations]
                           [:id :animation-ids]]
                          (map :image (:images atlas)) self)
      (map (partial attach-atlas-animation self) (:animations atlas)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "atlas"
                                    :label "Atlas"
                                    :build-ext "texturesetc"
                                    :node-type AtlasNode
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}))

(defn- selection->atlas [selection] (handler/adapt-single selection AtlasNode))
(defn- selection->animation [selection] (handler/adapt-single selection AtlasAnimation))
(defn- selection->image [selection] (handler/adapt-single selection AtlasImage))

(def ^:private default-animation
  {:flip-horizontal false
   :flip-vertical false
   :fps 60
   :playback :playback-loop-forward
   :id "New Animation"})

(defn- add-animation-group-handler [atlas-node]
  (g/transact
   (concat
    (g/operation-label "Add Animation Group")
    (attach-atlas-animation atlas-node default-animation))))

(handler/defhandler :add :workbench
  (label [] "Add Animation Group")
  (active? [selection] (selection->atlas selection))
  (run [selection] (add-animation-group-handler (selection->atlas selection))))

(defn- add-images-handler [workspace project labels parent scope-node] ; parent = new parent of images
  (when-let [images (seq (dialogs/make-resource-dialog workspace project {:ext image/exts :title "Select Images" :selection :multiple}))]
    (g/transact
     (concat
      (g/operation-label "Add Images")
      (attach-image-nodes parent labels images scope-node)))))

(handler/defhandler :add-from-file :workbench
  (label [] "Add Images...")
  (active? [selection] (or (selection->atlas selection) (selection->animation selection)))
  (run [project selection] (let [workspace (project/workspace project)]
                             (if-let [atlas-node (selection->atlas selection)]
                               (add-images-handler workspace project
                                                   [[:animation :animations]
                                                    [:id :animation-ids]]
                                                   atlas-node atlas-node))
                             (when-let [animation-node (selection->animation selection)]
                               (add-images-handler workspace [[:image :frames]] animation-node (core/scope animation-node))))))

(defn- targets-of [node label]
  (keep
   (fn [[src src-lbl trg trg-lbl]]
     (when (= src-lbl label)
       [trg trg-lbl]))
   (g/outputs node)))

(defn- image-parent [node]
  (first (first (targets-of node :image-order))))

(defn- move-active? [selection]
  (some->> selection
    selection->image
    image-parent
    (g/node-instance? AtlasAnimation)))

(defn- run-move [selection direction]
  (let [image (selection->image selection)
        parent (image-parent image)
        order-delta (if (= direction :move-up) -1 1)
        new-image-order (->> (g/node-value parent :image-order) ; original image order
                          (sort-by second) ; sort by order
                          (map-indexed (fn [ix [node _]] [node (* 2 ix)])) ; compact order + make space
                          (map (fn [[node ix]] [node (if (= image node) (+ ix (* 3 order-delta)) ix)])) ; move image to space before previous or after successor
                          (sort-by second))]
    (g/transact
      (map (fn [[image order]]
             (g/set-property image :order order))
           new-image-order))))

(handler/defhandler :move-up :workbench
  (active? [selection] (move-active? selection))
  (run [selection] (run-move selection :move-up)))

(handler/defhandler :move-down :workbench
  (active? [selection] (move-active? selection))
  (run [selection] (run-move selection :move-down)))
