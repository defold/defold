(ns dynamo.texture
  (:require [dynamo.types :refer :all]
            [dynamo.condition :refer :all]
            [dynamo.image :refer :all]
            [dynamo.node :refer [defnode]]
            [internal.texture.pack-max-rects :refer [max-rects-packing]]
            [internal.texture.engine :refer [texture-engine-format-generate]]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]])
  (:import [java.awt.image BufferedImage]
           [dynamo.types Rect Image TextureSet EngineFormatTexture]))

(set! *warn-on-reflection* true)

(defnk animation-frames [this g])

(sm/defn blank-textureset :- TextureSet
  ([]
    (blank-textureset 64 64 0.9568 0.0 0.6313))
  ([w :- s/Num h :- s/Num r :- Float g :- Float b :- Float]
    (let [rct   (rect 0 0 w h)
          img (flood (blank-image w h) r g b)]
      (TextureSet. rct img [rct] [rct] []))))


(defnode AnimationBehavior
  (input images [Image])

  (property fps             (non-negative-integer :default 30))
  (property flip-horizontal (bool))
  (property flip-vertical   (bool))
  (property playback        (as-schema AnimationPlayback))

  (output frames s/Any animation-frames))

(sm/defn make-empty-textureset :- TextureSet
  []
  (TextureSet. (rect 0 0 16 16) (blank-image 16 16) [] [] []))

(sm/defn pack-textures :- TextureSet
  [margin    :- (s/maybe s/Int)
   extrusion :- (s/maybe s/Int)
   sources   :- [Image]]
  (if (empty? sources)
    (signal :empty-source-list)
    (let [extrusion     (max 0 (or extrusion 0))
          margin        (max 0 (or margin 0))
          sources       (map (partial extrude-borders extrusion) sources)
          textureset    (max-rects-packing margin (map image-bounds sources))
          texture-image (composite (blank-image (:aabb textureset)) (:coords textureset) sources)]
      (assoc textureset :packed-image texture-image))))

(sm/defn ->engine-format :- EngineFormatTexture
  [original :- BufferedImage]
  (texture-engine-format-generate original))

(doseq [[v doc]
       {*ns*
        "Schema, behavior, and type information related to textures."

        #'animation-frames
        "Returns the frames of the animation."

        #'pack-textures
        "Returns a TextureSet. Margin and extrusion is applied, then the sources are packed.
         \n\nSignals :empty-source-list if the list of sources is empty."

        #'blank-textureset
        "Create a blank TextureSet with the specified width w, height h, and color values (r g b). Color values should be between 0 and 1.0."
        }]
  (alter-meta! v assoc :doc doc))
