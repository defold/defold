(ns editor.texture
  "Schema, behavior, and type information related to textures."
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.image-util :refer [flood blank-image extrude-borders image-bounds composite]]
            [editor.types :as types]
            [editor.texture.engine :refer [texture-engine-format-generate]]
            [editor.texture.pack-max-rects :refer [max-rects-packing]])
  (:import [editor.types Rect Image TexturePacking EngineFormatTexture]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(defn- basename [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn animation-from-image [image]
  (types/map->Animation {:id              (basename (:path image))
                   :images          [image]
                   :fps             30
                   :flip-horizontal 0
                   :flip-vertical   0
                   :playback        :PLAYBACK_ONCE_FORWARD}))

(defn image-from-rect [rect]
  (types/map->Image (select-keys rect [:path :width :height])))

(s/defn blank-texture-packing :- TexturePacking
  "Create a blank TexturePacking with the specified width w, height h, and color values (r g b). Color values should be between 0 and 1.0."
  ([]
    (blank-texture-packing 64 64 0.9568 0.0 0.6313))
  ([w :- s/Num h :- s/Num r :- Float g :- Float b :- Float]
    (let [rct       (assoc (types/rect 0 0 w h) :path "placeholder")
          img       (flood (blank-image w h) r g b)
          image     (assoc (image-from-rect rct) :contents img)
          animation (animation-from-image image)]
      (TexturePacking. rct img [rct] [rct] [animation]))))

(s/defn pack-textures :- TexturePacking
  "Returns a TexturePacking. Margin and extrusion is applied, then the sources are packed."
  [margin    :- (s/maybe s/Int)
   extrusion :- (s/maybe s/Int)
   sources   :- [Image]]
  (assert (seq sources) "sources must be non-empty seq of images.")
  (let [extrusion       (max 0 (or extrusion 0))
        margin          (max 0 (or margin 0))
        sources         (map (partial extrude-borders extrusion) sources)
        texture-packing (max-rects-packing margin (map image-bounds sources))
        texture-image   (composite (blank-image (:aabb texture-packing)) (:coords texture-packing) sources)]
    (assoc texture-packing :packed-image texture-image)))

(s/defn ->engine-format :- EngineFormatTexture
  [original :- BufferedImage]
  (texture-engine-format-generate original))
