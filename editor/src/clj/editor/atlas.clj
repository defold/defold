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
            [editor.project :as project]
            [editor.texture :as tex]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.scene :as scene]
            [editor.outline :as outline]
            [editor.validation :as validation]
            [editor.gl.pass :as pass])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
           [com.dynamo.tile.proto Tile$Playback]
           [com.jogamp.opengl.util.awt TextRenderer]
           [com.google.protobuf ByteString]
           [editor.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]
           [java.nio ByteBuffer ByteOrder FloatBuffer]))

(set! *warn-on-reflection* true)

(def atlas-icon "icons/32/Icons_13-Atlas.png")
(def animation-icon "icons/32/Icons_24-AT-Animation.png")
(def image-icon "icons/32/Icons_25-AT-Image.png")

(defn render-overlay
  [^GL2 gl width height]
  (scene/overlay-text gl (format "Size: %dx%d" width height) 12.0 -22.0))

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

(defn- path->id [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn image->animation [image]
  (types/map->Animation {:id              (path->id (:path image))
                         :images          [image]
                         :fps             30
                         :flip-horizontal false
                         :flip-vertical   false
                         :playback        :playback-once-forward}))

(g/defnode AtlasImage
  (inherits outline/OutlineNode)

  (property order g/Int (dynamic visible (g/always false)) (default 0))
  (input src-resource (g/protocol resource/Resource))
  (input src-image BufferedImage)
  (output image-order g/Any (g/fnk [_node-id order] [_node-id order]))
  (output path g/Str (g/fnk [src-resource] (resource/proj-path src-resource)))
  (output image Image (g/fnk [path ^BufferedImage src-image] (Image. path src-image (.getWidth src-image) (.getHeight src-image))))
  (output animation Animation (g/fnk [image] (image->animation image)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id path order] {:node-id _node-id
                                                                                 :label (format "%s - %s" (path->id path) path)
                                                                                 :order order
                                                                                 :icon image-icon}))
  (output ddf-message g/Any :cached (g/fnk [path order] {:image path :order order})))

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

(defn- attach-image [parent tgt-label image-node src-label scope-node image-order]
  (concat
   (g/set-property image-node :order image-order)
   (g/connect image-node :image-order  parent     :image-order)
   (g/connect image-node :_node-id     scope-node :nodes)
   (g/connect image-node src-label     parent     tgt-label)
   (g/connect image-node :node-outline parent     :child-outlines)
   (g/connect image-node :ddf-message  parent     :img-ddf)))

(defn- attach-animation [atlas-node animation-node]
  (concat
   (g/connect animation-node :_node-id     atlas-node :nodes)
   (g/connect animation-node :animation    atlas-node :animations)
   (g/connect animation-node :node-outline atlas-node :child-outlines)
   (g/connect animation-node :ddf-message  atlas-node :anim-ddf)))

(defn- next-image-order [parent]
  (inc (apply max -1 (map second (g/node-value parent :image-order)))))

(defn- tx-attach-image-to-animation [animation-node image-node]
  (attach-image animation-node
                :frames
                image-node
                :image
                (core/scope animation-node)
                (next-image-order animation-node)))

(g/defnode AtlasAnimation
  (inherits outline/OutlineNode)

  (property id  g/Str)
  (property fps g/Int
            (default 30)
            (validate (validation/validate-positive fps "FPS must be greater than or equal to zero")))
  (property flip-horizontal g/Bool)
  (property flip-vertical   g/Bool)
  (property playback        types/AnimationPlayback
            (dynamic edit-type (g/always
                                 (let [options (protobuf/enum-values Tile$Playback)]
                                   {:type :choicebox
                                    :options (zipmap (map first options)
                                                     (map (comp :display-name second) options))}))))

  (input frames Image :array)
  (input img-ddf g/Any :array)

  (input image-order g/Any :array)

  (output animation Animation (g/fnk [this id frames fps flip-horizontal flip-vertical playback]
                                     (types/->Animation id frames fps flip-horizontal flip-vertical playback)))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id child-outlines] {:node-id _node-id
                                         :label id
                                         :children (sort-by :order child-outlines)
                                         :icon animation-icon
                                         :child-reqs [{:node-type AtlasImage
                                                       :tx-attach-fn tx-attach-image-to-animation}]}))
  (output ddf-message g/Any :cached produce-anim-ddf))

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

(defn- build-texture-set [self basis resource dep-resources user-data]
  (let [tex-set (assoc (:proto user-data) :texture (resource/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes TextureSetProto$TextureSet tex-set)}))

(g/defnk produce-build-targets [_node-id resource texture-set-data save-data]
  (let [project          (project/get-project _node-id)
        workspace        (project/workspace project)
        texture-target   (image/make-texture-build-target workspace _node-id (:image texture-set-data))]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture-set
      :user-data {:proto (:texture-set texture-set-data)}
      :deps [texture-target]}]))

(defn gen-renderable-vertex-buffer
  [width height]
  (let [x0 0
        y0 0
        x1 width
        y1 height]
    (persistent!
      (doto (->texture-vtx 6)
           (conj! [x0 y0 0 1 0 1])
           (conj! [x0 y1 0 1 0 0])
           (conj! [x1 y1 0 1 1 0])

           (conj! [x1 y1 0 1 1 0])
           (conj! [x1 y0 0 1 1 1])
           (conj! [x0 y0 0 1 0 1])))))

(g/defnk produce-scene
  [_node-id texture-set-data aabb gpu-texture]
  (let [^BufferedImage img (:image texture-set-data)
        width (.getWidth img)
        height (.getHeight img)
        vertex-buffer (gen-renderable-vertex-buffer width height)
        vertex-binding (vtx/use-with _node-id vertex-buffer atlas-shader)]
    {:aabb aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (let [pass (:pass render-args)]
                                 (cond
                                   (= pass pass/overlay)     (render-overlay gl width height)
                                   (= pass pass/transparent) (render-texture-set gl render-args vertex-binding gpu-texture))))
                  :passes [pass/overlay pass/transparent]}}))

(g/defnk produce-texture-set-data [animations images margin inner-padding extrude-borders]
  (texture-set-gen/->texture-set-data animations images margin inner-padding extrude-borders))

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

(defn- tx-attach-image-to-atlas [atlas-node image-node]
  (attach-image atlas-node
                :animations
                image-node
                :animation
                atlas-node
                (next-image-order atlas-node)))

(defn- atlas-outline-sort-by-fn [v]
  [(:name (g/node-type* (:node-id v)))])

(g/defnode AtlasNode
  (inherits project/ResourceNode)

  (property margin g/Int
            (default 0)
            (validate (validation/validate-positive margin "Margin must be greater than or equal to zero")))
  (property inner-padding g/Int
            (default 0)
            (validate (validation/validate-positive inner-padding "Inner padding must be greater than or equal to zero")))
  (property extrude-borders g/Int
            (default 0)
            (validate (validation/validate-positive extrude-borders "Extrude borders must be greater than or equal to zero")))

  (input animations Animation :array)
  (input img-ddf g/Any :array)
  (input anim-ddf g/Any :array)

  (input image-order g/Any :array)

  (output images           [Image]             :cached (g/fnk [animations] (vals (into {} (map (fn [img] [(:path img) img]) (mapcat :images animations))))))
  (output aabb             AABB                (g/fnk [texture-set-data] (let [^BufferedImage img (:image texture-set-data)] (types/->AABB (Point3d. 0 0 0) (Point3d. (.getWidth img) (.getHeight img) 0)))))
  (output gpu-texture      g/Any               :cached (g/fnk [_node-id texture-set-data] (texture/image-texture _node-id (:image texture-set-data))))
  (output texture-set-data g/Any               :cached produce-texture-set-data)
  (output packed-image     BufferedImage       (g/fnk [texture-set-data] (:image texture-set-data)))
  (output anim-data        g/Any               :cached produce-anim-data)
  (output node-outline     outline/OutlineData :cached (g/fnk [_node-id child-outlines] {:node-id _node-id
                                                                                         :label "Atlas"
                                                                                         :children (sort-by atlas-outline-sort-by-fn child-outlines)
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
  [parent tgt-label images src-label scope-node]
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
            (attach-image parent tgt-label atlas-image src-label scope-node image-order))))
       images))))


(defn add-images [atlas-node img-resources]
  ; used by tests
  (attach-image-nodes atlas-node :animations img-resources :animation atlas-node))

(defn- attach-atlas-animation [atlas-node anim]
  (let [graph-id (g/node-id->graph-id atlas-node)]
    (g/make-nodes
     graph-id
     [atlas-anim [AtlasAnimation :flip-horizontal (not= 0 (:flip-horizontal anim)) :flip-vertical (not= 0 (:flip-vertical anim))
                  :fps (:fps anim) :playback (:playback anim) :id (:id anim)]]
     (attach-animation atlas-node atlas-anim)
     (attach-image-nodes atlas-anim :frames (map :image (:images anim)) :image atlas-node))))

(defn load-atlas [project self resources]
  (let [atlas         (protobuf/read-text AtlasProto$Atlas resources)
        graph-id      (g/node-id->graph-id self)]
    (concat
      (g/set-property self :margin (:margin atlas))
      (g/set-property self :inner-padding (:inner-padding atlas))
      (g/set-property self :extrude-borders (:extrude-borders atlas))
      (attach-image-nodes self :animations (map :image (:images atlas)) :animation self)
      (map (partial attach-atlas-animation self) (:animations atlas)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "atlas"
                                    :label "Atlas"
                                    :build-ext "texturesetc"
                                    :node-type AtlasNode
                                    :load-fn load-atlas
                                    :icon atlas-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))

(defn- single-selection [selection]
  (= 1 (count selection)))

(defn- get-single-selection [node-type selection]
  (when (and (single-selection selection)
             (g/node-instance? node-type (first selection)))
    (first selection)))

(defn- atlas-selection [selection] (get-single-selection AtlasNode selection))
(defn- animation-selection [selection] (get-single-selection AtlasAnimation selection))
(defn- image-selection [selection] (get-single-selection AtlasImage selection))

(def ^:private default-animation
  {:flip-horizontal false
   :flip-vertical false
   :fps 60
   :playback :playback-loop-forward
   :id "New Animation"})

(defn- add-animation-group-handler [[atlas-node]]
  (g/transact
   (concat
    (g/operation-label "Add Animation Group")
    (attach-atlas-animation atlas-node default-animation))))

(handler/defhandler :add :global
  (label [] "Add Animation Group")
  (active? [selection] (atlas-selection selection))
  (run [selection] (add-animation-group-handler selection)))

(defn- add-images-handler [workspace src-label parent tgt-label scope-node] ; parent = new parent of images
  (when-let [images (seq (dialogs/make-resource-dialog workspace {:ext ["jpg" "png"] :title "Select Images" :selection :multiple}))]
    (g/transact
     (concat
      (g/operation-label "Add Images")
      (attach-image-nodes parent tgt-label images src-label scope-node)))))

(handler/defhandler :add-from-file :global
  (label [] "Add Images...")
  (active? [selection] (or (atlas-selection selection) (animation-selection selection)))
  (run [project selection] (let [workspace (project/workspace project)]
                             (if-let [atlas-node (atlas-selection selection)]
                               (add-images-handler workspace :animation atlas-node :animations atlas-node))
                             (when-let [animation-node (animation-selection selection)]
                               (add-images-handler workspace :image animation-node :frames (core/scope animation-node))))))

(defn- targets-of [node label]
  (keep
   (fn [[src src-lbl trg trg-lbl]]
     (when (= src-lbl label)
       [trg trg-lbl]))
   (g/outputs node)))

(defn- image-parent [node]
  (first (first (targets-of node :image-order))))

(defn- move-enabled? [selection]
  (image-selection selection))

(defn- move-active? [selection]
  (when-let [image (image-selection selection)]
    (g/node-instance? AtlasAnimation (image-parent image))))

(defn- move-image [parent image direction]
  (let [order-delta (if (= direction :move-up) -1 1)
        new-image-order (->> (g/node-value parent :image-order) ; original image order
                             (sort-by second) ; sort by order
                             (map-indexed (fn [ix [node _]] [node (* 2 ix)])) ; compact order + make space
                             (map (fn [[node ix]] [node (if (= image node) (+ ix (* 3 order-delta)) ix)])) ; move image to space before previous or after successor
                             (sort-by second))] ; sort according to new order
    (g/transact
     (map (fn [[image order]]
            (g/set-property image :order order))
          new-image-order))))

(defn- run-move [selection direction]
  (let [image (image-selection selection)
        parent (image-parent image)]
    (move-image parent image direction)))

(handler/defhandler :move-up :global
  (enabled? [selection] (move-enabled? selection))
  (active? [selection] (move-active? selection))
  (run [selection] (run-move selection :move-up)))

(handler/defhandler :move-down :global
  (enabled? [selection] (move-enabled? selection))
  (active? [selection] (move-active? selection))
  (run [selection] (run-move selection :move-down)))
 
(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Atlas"
                  :id ::atlas
                  :children [{:label "Move Up"
                              :acc "Alt+UP"
                              :command :move-up
                              }
                             {:label "Move Down"
                              :acc "Alt+DOWN"
                              :command :move-down
                              }
                             ]}])
