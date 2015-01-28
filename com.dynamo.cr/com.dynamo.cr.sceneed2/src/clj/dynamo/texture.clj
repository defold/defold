(ns dynamo.texture
  "Schema, behavior, and type information related to textures."
  (:require [dynamo.types :refer :all]
              [dynamo.image :refer :all]
              [dynamo.node :as n]
              [dynamo.property :as dp]
              [internal.texture.pack-max-rects :refer [max-rects-packing]]
              [internal.texture.engine :refer [texture-engine-format-generate]]
              [schema.core :as s]
              [schema.macros :as sm]
              [plumbing.core :refer [defnk]])
  (:import [java.awt.image BufferedImage]
           [dynamo.types Rect Image TextureSet EngineFormatTexture]))

(set! *warn-on-reflection* true)

(sm/defn blank-textureset :- TextureSet
  "Create a blank TextureSet with the specified width w, height h, and color values (r g b). Color values should be between 0 and 1.0."
  ([]
    (blank-textureset 64 64 0.9568 0.0 0.6313))
  ([w :- s/Num h :- s/Num r :- Float g :- Float b :- Float]
    (let [rct (rect 0 0 w h)
          img (flood (blank-image w h) r g b)]
      (TextureSet. rct img [rct] [rct] []))))

(sm/defn make-empty-textureset :- TextureSet
  []
  (TextureSet. (rect 0 0 16 16) (blank-image 16 16) [] [] []))

(sm/defn pack-textures :- TextureSet
  "Returns a TextureSet. Margin and extrusion is applied, then the sources are packed."
  [margin    :- (s/maybe s/Int)
   extrusion :- (s/maybe s/Int)
   sources   :- [Image]]
  (assert (seq sources) "sources must be non-empty seq of images.")
  (let [extrusion     (max 0 (or extrusion 0))
        margin        (max 0 (or margin 0))
        sources       (map (partial extrude-borders extrusion) sources)
        textureset    (max-rects-packing margin (map image-bounds sources))
        texture-image (composite (blank-image (:aabb textureset)) (:coords textureset) sources)]
    (assoc textureset :packed-image texture-image)))

(sm/defn ->engine-format :- EngineFormatTexture
  [original :- BufferedImage]
  (texture-engine-format-generate original))

