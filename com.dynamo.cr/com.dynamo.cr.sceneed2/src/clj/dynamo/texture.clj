(ns dynamo.texture
  (:require [dynamo.types :refer :all]
            [dynamo.image :refer :all]
            [internal.texture.pack-max-rects :refer [max-rects-packing]]
            [internal.texture.engine :refer [texture-engine-format-generate]]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]])
  (:import [java.awt.image BufferedImage]
           [dynamo.types Rect Image TextureSet EngineFormatTexture]))

;; Value
(defnk animation-frames [this g])

(def AnimationBehavior
  {:inputs     {:images (as-schema [ImageSource])}
   :properties {:fps (non-negative-integer :default 30)
                :flip-horizontal {:schema s/Bool}
                :flip-vertical {:schema s/Bool}
                :playback (as-schema AnimationPlayback)}
   :transforms {:frames #'animation-frames}})

(sm/defn pack-textures :- TextureSet
  [margin    :- (s/maybe s/Int)
   extrusion :- (s/maybe s/Int)
   sources   :- [Image]]
  (let [extrusion     (max 0 (or extrusion 0))
        margin        (max 0 (or margin 0))
        sources       (map (partial extrude-borders extrusion) sources)
        textureset    (max-rects-packing margin (map image-bounds sources))
        texture-image (composite (blank-image (:aabb textureset)) (:coords textureset) sources)]
    (assoc textureset :packed-image texture-image)))

(sm/defn ->engine-format :- EngineFormatTexture
  [original :- BufferedImage]
  (texture-engine-format-generate original))

(doseq [[v doc]
       {*ns*
        "Schema, behavior, and type information related to textures."

        #'AnimationBehavior
        "*behavior* - Used by [[dynamo.node/defnode]] to define the behavior for an animation resource.

Inputs:

* `:images` - vector of [[Image]]

Properties:

* `:fps` - non-negative integer, default 30

Transforms:

* `:frames` - see [[animation-frames]]"

        #'animation-frames
        "Returns the frames of the animation."

        #'pack-textures
        ""
        }]
  (alter-meta! v assoc :doc doc))
