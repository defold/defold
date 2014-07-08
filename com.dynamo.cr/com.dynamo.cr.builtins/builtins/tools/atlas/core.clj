(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [dynamo.node :refer :all]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters]]
            [dynamo.project :refer [*current-project* register-loader transact new-resource connect query]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.outline :refer :all]
            [camel-snake-kebab :refer :all])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.textureset.proto TextureSetProto$TextureSet TextureSetProto$TextureSet$Builder
                                         TextureSetProto$TextureSetAnimation TextureSetProto$TextureSetAnimation$Builder]
            [com.google.protobuf ByteString]))

(defnode ImageNode
 OutlineNode
 Image)

(defnode AnimationGroupNode
  OutlineNode
  Animation)

(def ConvexHull
  {:index           int
   :count           int
   :collision-group s/Str})

(def Playback
  (s/enum :PLAYBACK_NONE :PLAYBACK_ONCE_FORWARD :PLAYBACK_ONCE_BACKWARD :PLAYBACK_ONCE_PINGPONG
          :PLAYBACK_LOOP_FORWARD :PLAYBACK_LOOP_BACKWARD :PLAYBACK_LOOP_PINGPONG))

(def TextureSetAnimation
  {:id                               s/Str
   :width                            int
   :height                           int
   :start                            int
   :end                              int
   (s/optional-key :fps)             int
   (s/optional-key :playback)        Playback
   (s/optional-key :flip-horizontal) int
   (s/optional-key :flip-vertical)   int
   (s/optional-key :is-animation)    int})

(def TextureSet
  {:texture                      s/Str
   :animations                   [TextureSetAnimation]
   :convex-hulls                 [ConvexHull]
   (s/optional-key :tile-width)  int
   (s/optional-key :tile-height) int

   :vertices                     bytes
   :vertex-start                 [int]
   :vertex-count                 [int]

   :outline-vertices             bytes
   :outline-vertex-start         [int]
   :outline-vertex-count         [int]

   :convex-hull-points           [float]
   :collision-groups             [s/Str]

   :tex-coords                   bytes
   :tile-count                   int})

(defnk produce-textureset :- TextureSet
  [g this images :- [TextureImage] animations :- [TextureSetAnimation]]
  {:texture "a-texture"
   :animations []
   :convex-hulls []
   :vertices (byte-array 0)
   :vertex-start [(int 0)]
   :vertex-count [(int 0)]
   :outline-vertices (byte-array 0)
   :outline-vertex-start []
   :outline-vertex-count []
   :convex-hull-points []
   :collision-groups []
   :tex-coords (byte-array 0)
   :tile-count 99})

(def AtlasProperties
  {:inputs     {:assets     [OutlineItem]
                :images     [TextureImage]
                :animations [Animation]}
   :properties {:margin (non-negative-integer)
                :extrude-borders (non-negative-integer)
                :filename (string)}
   :transforms {:textureset #'produce-textureset}
   :cached     #{:textureset}})

(defnode AtlasNode
  OutlineNode
  AtlasProperties)

(protocol-buffer-converters
 AtlasProto$Atlas
 {:constructor #'atlas.core/make-atlas-node
  :basic-properties [:extrude-borders :margin]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]
                     :animations-list [:tree -> :children]}}

 AtlasProto$AtlasAnimation
 {:constructor      #'atlas.core/make-animation-group-node
  :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]}}

 AtlasProto$AtlasImage
 {:constructor #'atlas.core/make-image-node
  :basic-properties [:image]})

(defn- set-if-present
  ([inst k props]
    (set-if-present inst k props identity))
  ([inst k props xform]
    (when-let [value (k props)]
      (. inst (symbol (->camelCase (str "set-" k))) (xform value)))))

(defn- build-animation
  [anim]
  (.build (doto (TextureSetProto$TextureSetAnimation/newBuilder)
             (.setId        (:id anim))
             (.setEnd       (:end anim))
             (.setWidth     (:width anim))
             (.setHeight    (:height anim))
             (set-if-present :playback anim)
             (set-if-present :fps anim)
             (set-if-present :flip-horizontal anim)
             (set-if-present :flip-vertical anim)
             (set-if-present :is-animation anim))))

(defn- build-animations
  [animations]
  (map build-animation animations))

(defn- map->TextureSet
  [textureset]
  (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               (:texture textureset))
            (.setTexCoords             (ByteString/copyFrom (:tex-coords textureset)))
            (.setTileCount             (int (:tile-count textureset)))
            (set-if-present            :tile-width textureset int)
            (set-if-present            :tile-height textureset int)
            (.addAllVertexStart        (:vertex-start textureset))
            (.addAllVertexCount        (:vertex-count textureset))
            (.setVertices              (ByteString/copyFrom (:vertices textureset)))
            (.addAllOutlineVertexStart (:outline-vertex-start textureset))
            (.addAllOutlineVertexCount (:outline-vertex-count textureset))
            (.setOutlineVertices       (ByteString/copyFrom (:outline-vertices textureset)))
            (.addAllAnimations         (build-animations (:animations textureset))))))

(defnk compile-texturesetc :- s/Bool
  [this g project textureset :- TextureSet]
  (write-native-file (:textureset-filename this) (.toByteArray (map->TextureSet textureset))))

(defnk compile-texturec :- s/Bool
  [this g textureset :- TextureSet]
  true)

(def TextureCompiler
  {:inputs     {:textureset (as-schema TextureSet)}
   :properties {:texture-filename {:schema s/Any :default ""}}
   :transforms {:texturec    #'compile-texturec}
   :on-update  #{:texturec}})

(def TextureSetCompiler
  {:inputs     {:textureset (as-schema TextureSet)}
   :properties {:textureset-filename {:schema s/Any :default ""}}
   :transforms {:texturesetc #'compile-texturesetc}
   :on-update  #{:texturesetc}})

(defnode AtlasCompiler
  TextureCompiler
  TextureSetCompiler)



(defn on-load
  [project path ^AtlasProto$Atlas atlas-message]
  (let [atlas-tx (message->node atlas-message nil nil nil :filename (local-path path) :_id -1)
        compiler (make-atlas-compiler :textureset-filename (in-build-directory (replace-extension path "texturesetc"))
                                      :texture-filename    (in-build-directory (replace-extension path "texturec")))]
    (transact project
      [atlas-tx
       (new-resource compiler)
       (connect {:_id -1} :textureset compiler :textureset)])))

(register-loader *current-project* "atlas" (protocol-buffer-loader AtlasProto$Atlas on-load))

(comment

  (dynamo.project/load-resource *project* "/stars/new_file.atlas")

  (-> "/stars/new_file.atlas"
    convert-to-project-path
    input-stream
    (as-protocol-buffer AtlasProto$Atlas)
    message->node
    transact)


  ;; current-selection returns [graph [selected-node-ids]]
  ;; context menu action
  (defn add-image [^ExecutionEvent event]
    (let [resource   (select-resource IMAGE)
          image-node (make-image-node resource)
          target     (current-selection event)]
      (add-to-assets target image-node :image)))

  (defcommand add-image-command "dynamo.atlas" "dynamo.atlas.add-image")
  (defhandler add-image-handler add-image-command #'atlas.core/add-image))
