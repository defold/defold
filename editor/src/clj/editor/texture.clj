(ns editor.texture
  "Schema, behavior, and type information related to textures."
  (:require [clojure.string :as str]
            [editor.image :refer [flood blank-image extrude-borders image-bounds composite]]
            [dynamo.types :as t]
            [dynamo.types :refer [map->Animation map->Image rect]]
            [internal.texture.engine :refer [texture-engine-format-generate]]
            [internal.texture.pack-max-rects :refer [max-rects-packing]]
            [schema.core :as s])
  (:import [dynamo.types Rect Image TexturePacking EngineFormatTexture]
           [java.awt.image BufferedImage]))

(defn- basename [path]
  (-> path
      (str/split #"/")
      last
      (str/split #"\.(?=[^\.]+$)")
      first))

(defn animation-from-image [image]
  (map->Animation {:id              (basename (:path image))
                   :images          [image]
                   :fps             30
                   :flip-horizontal 0
                   :flip-vertical   0
                   :playback        :PLAYBACK_ONCE_FORWARD}))

(defn image-from-rect [rect]
  (map->Image (select-keys rect [:path :width :height])))

(s/defn blank-texture-packing :- TexturePacking
  "Create a blank TexturePacking with the specified width w, height h, and color values (r g b). Color values should be between 0 and 1.0."
  ([]
    (blank-texture-packing 64 64 0.9568 0.0 0.6313))
  ([w :- t/Num h :- t/Num r :- Float g :- Float b :- Float]
    (let [rct       (assoc (rect 0 0 w h) :path "placeholder")
          img       (flood (blank-image w h) r g b)
          image     (assoc (image-from-rect rct) :contents img)
          animation (animation-from-image image)]
      (TexturePacking. rct img [rct] [rct] [animation]))))

(s/defn pack-textures :- TexturePacking
  "Returns a TexturePacking. Margin and extrusion is applied, then the sources are packed."
  [margin    :- (t/maybe t/Int)
   extrusion :- (t/maybe t/Int)
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
