(ns dynamo.types
  (:require [schema.core :as s]
            [schema.macros :as sm])
  (:import [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.tile.proto Tile$Playback]))

; ----------------------------------------
; Functions to create basic value types
; ----------------------------------------

(defn as-schema   [x] (with-meta x {:schema true}))
(defn has-schema? [v] (and (fn? (if (var? v) (var-get v) v)) (:schema (meta v))))

(def Int32   (s/pred #(instance? java.lang.Integer %) 'int32?))
(def Icon    s/Str)
(def NodeRef s/Int)

(sm/defrecord Rect
  [path     :- s/Any
   x        :- Int32
   y        :- Int32
   width    :- Int32
   height   :- Int32])

(sm/defn ^:always-validate rect :- Rect
  ([x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (rect "" (int  x) (int y) (int width) (int height)))
  ([path :- s/Any x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (Rect. path (int x) (int y) (int width) (int height))))

(sm/defrecord Image
  [path     :- s/Any
   contents :- BufferedImage
   width    :- Int32
   height   :- Int32])

(def AnimationPlayback (s/enum :PLAYBACK_NONE :PLAYBACK_ONCE_FORWARD :PLAYBACK_ONCE_BACKWARD
                               :PLAYBACK_ONCE_PINGPONG :PLAYBACK_LOOP_FORWARD :PLAYBACK_LOOP_BACKWARD
                               :PLAYBACK_LOOP_PINGPONG))

(sm/defrecord Animation
  [id              :- s/Str
   images          :- [Image]
   fps             :- Int32
   flip-horizontal :- s/Bool
   flip-vertical   :- s/Bool
   playback        :- Tile$Playback])

(sm/defrecord TextureSet
  [aabb         :- Rect
   packed-image :- BufferedImage
   coords       :- [Rect]
   sources      :- [Rect]
   animations   :- [Animation]])

(sm/defrecord Vertices
  [counts   :- [Int32]
   starts   :- [Int32]
   vertices :- [s/Num]])

(sm/defrecord EngineFormatTexture
  [width           :- Int32
   height          :- Int32
   original-width  :- Int32
   original-height :- Int32
   format          :- Graphics$TextureImage$TextureFormat
   data            :- ByteBuffer
   mipmap-sizes    :- [Int32]
   mipmap-offsets  :- [Int32]])


(defn number               [& {:as opts}] (merge {:schema s/Num} opts))
(defn string               [& {:as opts}] (merge {:schema s/Str :default ""} opts))
(defn icon                 [& {:as opts}] (merge {:schema Icon} opts))
(defn resource             [& {:as opts}] (merge {:schema s/Str :default ""} opts))
(defn texture-image        [& {:as opts}] (merge {:schema bytes} opts))
(defn non-negative-integer [& {:as opts}] (merge (number :default 0) opts))
(defn isotropic-scale      [& {:as opts}] (merge (number :default 1.0) opts))

(doseq [[v doc]
       {*ns*                   "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
        #'as-schema            "applies schema metadata to x."
        #'has-schema?          "true if v has defined schema. That is, metadata includes a schema key."
        #'Icon                 "*schema* - schema for the representation of an Icon as s/Str"
        #'NodeRef              "*schema* - schema for the representation of a node reference as s/Int"
        #'number               "creates a property definition for a numeric property"
        #'string               "creates a property definition for a string property"
        #'icon                 "creates a property definition for an [[Icon]] property"
        #'resource             "creates a property definition for a resource (file)"
        #'texture-image        "creates a property definition for a byte array property to be used as an image"
        #'non-negative-integer "creates a property definition for a numeric property that must be zero or greater"
        #'isotropic-scale      "creates a property definition for a uniform scaling factor"}]
  (alter-meta! v assoc :doc doc))