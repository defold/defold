(ns editor.texture
  "Schema, behavior, and type information related to textures."
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.image :refer [flood blank-image extrude-borders composite]]
            [editor.types :as types]
            [editor.texture.engine :refer [texture-engine-format-generate]])
  (:import [editor.types Rect Image TexturePacking EngineFormatTexture]
           [java.awt.image BufferedImage]))

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

(g/s-defn ->engine-format :- EngineFormatTexture
  [original :- BufferedImage]
  (texture-engine-format-generate original))
