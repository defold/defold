(ns editor.tile-source
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.project :as project]
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
            [editor.validation :as validation])
  (:import [com.dynamo.tile.proto Tile$TileSet Tile$Playback]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.google.protobuf ByteString]
           [editor.types AABB]
           [javax.vecmath Point3d Matrix4d]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer ByteOrder FloatBuffer]))

(def atlas-icon "icons/32/Icons_47-Tilesource.png")
(def animation-icon "icons/32/Icons_24-AT-Animation.png")
(def collision-icon "icons/32/Icons_43-Tilesource-Collgroup.png")

; TODO - fix real profiles
(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(g/defnk produce-pb
  ;; this will be called both
  ;; * during save-data production with ignore-errors <severe> meaning f.i. image may be unvalidated (nil)
  ;; * during produce-build-targets with normal validation
  [image tile-width tile-height tile-margin tile-spacing collision material-tag convex-hulls convex-hull-points collision-groups animation-ddfs extrude-borders inner-padding :as all]
  (let [pb (-> all
               (dissoc :image :collision :_node-id :basis)
               (assoc :collision-groups (sort collision-groups)
                      :animations (sort-by :id animation-ddfs)))]
    (cond-> (assoc pb :image (resource/resource->proj-path image))
      collision (assoc :collision (resource/proj-path collision)))))

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

(g/defnk produce-texture-set-data [pb image image-content collision collision-content]
  (texture-set-gen/tile-source->texture-set-data pb image-content collision-content))

; TODO - copied from atlas, generalize into separate file - texture-set-util?

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
  (property id g/Str)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id] {:node-id _node-id :label id :icon collision-icon})))

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
  (concat
   (g/connect animation-node :_node-id self :nodes)
   (g/connect animation-node :node-outline self :child-outlines)
   (g/connect animation-node :ddf-message self :animation-ddfs)))

(defn- attach-collision-group-node [self collision-group-node]
  (concat
   (g/connect collision-group-node :_node-id self :nodes)
   (g/connect collision-group-node :node-outline self :child-outlines)
   (g/connect collision-group-node :id self :collision-groups)))

(g/defnk produce-tile-source-outline [_node-id child-outlines]
  {:node-id _node-id
   :label "Tile Source"
   :children (sort-by (fn [v] [(:name (g/node-type* (:node-id v))) (:label v)]) child-outlines)
   :child-reqs [{:node-type TileAnimationNode
                 :tx-attach-fn attach-animation-node}
                {:node-type CollisionGroupNode
                 :tx-attach-fn attach-collision-group-node}]
   })

(g/defnk produce-aabb [texture-set-data]
  (let [^BufferedImage img (:image texture-set-data)]
    (types/->AABB (Point3d. 0 0 0) (Point3d. (.getWidth img) (.getHeight img) 0))))

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

(def tile-shader (shader/make-shader ::tile-shader pos-uv-vert pos-uv-frag))

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

(defn- render-texture-set
  [^GL2 gl render-args vertex-binding gpu-texture]
  (gl/with-gl-bindings gl render-args [gpu-texture tile-shader vertex-binding]
    (shader/set-uniform tile-shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)))
                        
(g/defnk produce-scene [_node-id texture-set-data aabb gpu-texture]
  (let [^BufferedImage img (:image texture-set-data)
        width (.getWidth img)
        height (.getHeight img)
        vertex-buffer (gen-renderable-vertex-buffer width height)
        vertex-binding (vtx/use-with _node-id vertex-buffer tile-shader)]
    {:aabb aabb
     :renderable {:render-fn (fn [gl render-args renderables count]
                               (let [pass (:pass render-args)]
                                 (condp = pass
                                   pass/transparent (render-texture-set gl render-args vertex-binding gpu-texture))))
                  :passes [pass/transparent]
                  }
     }))

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

(g/defnode TileSourceNode
  (inherits project/ResourceNode)

  (property image (g/protocol resource/Resource)
    (value (gu/passthrough image-resource))
    (set (project/gen-resource-setter [[:resource :image-resource]
                                       [:content :image-content]]))
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
  
  (property collision (g/protocol resource/Resource) ; optional
    (value (gu/passthrough collision-resource))
    (set (project/gen-resource-setter [[:resource :collision-resource]
                                       [:content :collision-content]]))
    (validate (g/fnk [collision-resource collision-content]
                     ;; collision-resource is optional, but depend on resource & content to trigg auto error aggregation
                     )))
  
  (property material-tag g/Str (default "tile"))
  (property convex-hulls g/Any (dynamic visible (g/always false)))
  (property convex-hull-points g/Any (dynamic visible (g/always false)))
  (property extrude-borders g/Int (default 0))
  (property inner-padding g/Int (default 0))

  (input collision-groups g/Str :array)
  (input animation-ddfs g/Any :array)
  (input image-resource (g/protocol resource/Resource))
  (input image-content BufferedImage)
  (input collision-resource (g/protocol resource/Resource))
  (input collision-content BufferedImage)

  (output aabb AABB :cached produce-aabb)
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-tile-source-outline)
  (output pb g/Any :cached produce-pb)
  (output save-data g/Any :cached produce-save-data)
  (output texture-set-data g/Any :cached produce-texture-set-data)
  (output packed-image BufferedImage (g/fnk [texture-set-data] (:image texture-set-data)))
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-texture g/Any :cached (g/fnk [_node-id texture-set-data] (texture/image-texture _node-id (:image texture-set-data))))
  (output anim-data g/Any :cached produce-anim-data)) 

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

(defn- load-tile-source [project self resource]
  (let [tile-source (protobuf/read-text Tile$TileSet resource)
        image (workspace/resolve-resource resource (:image tile-source))
        collision (workspace/resolve-resource resource (:collision tile-source))]
    (concat
     (for [field [:tile-width :tile-height :tile-margin :tile-spacing :material-tag :convex-hulls :extrude-borders :inner-padding]]
       (g/set-property self field (field tile-source)))
     (map (partial make-animation-node self) (:animations tile-source))
     (map (partial make-collision-group-node self) (:collision-groups tile-source))
     (g/set-property self :image image)
     (g/set-property self :collision collision))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext ["tilesource" "tileset"]
                                    :label "Tile Source"
                                    :build-ext "texturesetc"
                                    :node-type TileSourceNode
                                    :load-fn load-tile-source
                                    :icon atlas-icon
                                    :view-types [:scene]))

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

(def ^:private default-collision-group "New Collision Group")

(defn- add-collision-group-node [self]
  (g/transact (make-collision-group-node self default-collision-group)))

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
              {:label "Collision"
               :icon collision-icon
               :command :add
               :user-data {:action add-collision-group-node}}
              ]))
  (run [selection user-data]
    ((:action user-data) (first selection))))
