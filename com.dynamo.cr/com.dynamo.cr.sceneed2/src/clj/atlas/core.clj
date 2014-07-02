(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [dynamo.node :refer :all]
            [dynamo.file :refer [register-loader protocol-buffer-loader message->node]]
            [dynamo.file.protobuf :refer [protocol-buffer-converters]]
            [dynamo.project :refer [transact new-resource connect]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.outline :refer :all])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]))

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
  nil
  )

(def AtlasProperties
  {:inputs {:assets     [OutlineItem]
            :images     [TextureImage]
            :animations [Animation]}
   :properties {:margin (non-negative-integer)
                :extrude-borders (non-negative-integer)
                :filename (string)}
   :transforms {:textureset #'produce-textureset}})

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

(defnk compile-texturesetc :- s/Bool
  [this g textureset :- TextureSet]
  true)

(defnk compile-texturec :- s/Bool
  [this g textureset :- TextureSet]
  true)

(def TextureCompiler
  {:inputs     {:textureset (as-schema TextureSet)}
   :properties {:texture-filename {:schema s/Str :default ""}}
   :transforms {:texturec    #'compile-texturec}
   :on-update  #{:texturec}})

(def TextureSetCompiler
  {:inputs     {:textureset (as-schema TextureSet)}
   :properties {:textureset-filename {:schema s/Str :default ""}}
   :transforms {:texturesetc #'compile-texturesetc}
   :on-update  #{:texturesetc}})

(defnode AtlasCompiler
  TextureCompiler
  TextureSetCompiler)

(defn build-path [x] x)
(defn replace-extension [x y] x)

(defn on-load
  [^String filename ^AtlasProto$Atlas atlas-message]
  (let [atlas-tx (transact (message->node atlas-message nil nil nil :filename filename))
        atlas    {}
        compiler (make-atlas-compiler :textureset-filename (build-path (replace-extension filename ".texturesetc"))
                                      :texture-filename    (build-path (replace-extension filename ".texturec")))]
    (transact
      [(new-resource compiler)
       (connect atlas :textureset compiler :textureset)])))

(register-loader ".atlas" (protocol-buffer-loader AtlasProto$Atlas on-load))

(comment

  (dynamo.file/load "/Users/mtnygard/src/defold/com.dynamo.cr/com.dynamo.cr.sceneed2/test/resources/test.atlas")

  ;; current-selection returns [graph [selected-node-ids]]
  ;; context menu action
  (defn add-image [^ExecutionEvent event]
    (let [resource   (select-resource IMAGE)
          image-node (make-image-node resource)
          target     (current-selection event)]
      (add-to-assets target image-node :image)))

  (defcommand add-image-command "dynamo.atlas" "dynamo.atlas.add-image")
  (defhandler add-image-handler add-image-command #'atlas.core/add-image))
