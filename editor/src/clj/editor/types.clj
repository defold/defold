(ns editor.types
  (:require [dynamo.graph :as g]
            [schema.core :as s])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.tile.proto Tile$Playback]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [javafx.scene Parent]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

;; ----------------------------------------
;; Protocols here help avoid circular dependencies
;; ----------------------------------------

(defprotocol R3Min
  (min-p ^Point3d  [this]))

(defprotocol R3Max
  (max-p ^Point3d  [this]))

(defprotocol Rotation
  (rotation ^Quat4d [this]))

(defprotocol Translation
  (translation ^Vector4d [this]))

(defprotocol Position
  (position ^Point3d [this]))

(defprotocol ImageHolder
  (contents ^BufferedImage [this]))

(defprotocol N2Extent
  (width ^Integer [this])
  (height ^Integer [this]))

(defprotocol Frame
  (frame [this] "Notified one frame after being altered."))

(defprotocol PathManipulation
  (^String           extension         [this]         "Returns the extension represented by this path.")
  (^PathManipulation replace-extension [this new-ext] "Returns a new path with the desired extension.")
  (^String           local-path        [this]         "Returns a string representation of the path and extension.")
  (^String           local-name        [this]         "Returns the last segment of the path"))

;;; ----------------------------------------
;;; Functions to create basic value types
;;; ----------------------------------------

;; note - Int32 is a schema, not a value type
(def Int32   (s/both s/Int (s/pred #(< Integer/MIN_VALUE % Integer/MAX_VALUE) 'int32?)))

(g/deftype Icon    s/Str)

(g/deftype Color   [s/Num])

(g/deftype Vec2    [(s/one s/Num "x")
                    (s/one s/Num "y")])

(g/deftype Vec3    [(s/one s/Num "x")
                    (s/one s/Num "y")
                    (s/one s/Num "z")])

(g/deftype Vec4    [(s/one s/Num "x")
                    (s/one s/Num "y")
                    (s/one s/Num "z")
                    (s/one s/Num "w")])

(defn Point3d->Vec3 [^Point3d p]
  [(.getX p) (.getY p) (.getZ p)])

(def Registry {s/Any s/Any})

(s/defrecord Rect
  [path     :- s/Any
   x        :- Int32
   y        :- Int32
   width    :- Int32
   height   :- Int32]
  N2Extent
  (width [this] width)
  (height [this] height))

(s/defrecord AABB [min max]
  R3Min
  (min-p [this] (.min this))
  R3Max
  (max-p [this] (.max this)))

(defmethod print-method AABB
  [^AABB v ^java.io.Writer w]
  (.write w (str "<AABB \"min: " (.min v) ", max: " (.max v) "\">")))

(s/defn ^:always-validate rect :- Rect
  ([x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (rect "" (int  x) (int y) (int width) (int height)))
  ([path :- s/Any x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (Rect. path (int x) (int y) (int width) (int height))))

(s/defrecord Image
  [path     :- s/Any
   contents :- (s/maybe BufferedImage)
   width    :- Int32
   height   :- Int32]
  ImageHolder
  (contents [this] contents))

(def ^:private playback-modes (s/enum :playback-none :playback-once-forward :playback-once-backward
                                     :playback-once-pingpong :playback-loop-forward :playback-loop-backward
                                     :playback-loop-pingpong))
(g/deftype AnimationPlayback playback-modes)

(s/defrecord Animation
  [id              :- s/Str
   images          :- [Image]
   fps             :- Int32
   flip-horizontal :- s/Bool
   flip-vertical   :- s/Bool
   playback        :- playback-modes])

(s/defrecord TexturePacking
  [aabb         :- Rect
   packed-image :- BufferedImage
   coords       :- [Rect]
   sources      :- [Rect]
   animations   :- [Animation]])

(s/defrecord Vertices
  [counts   :- [Int32]
   starts   :- [Int32]
   vertices :- [s/Num]])

(s/defrecord EngineFormatTexture
  [width           :- Int32
   height          :- Int32
   original-width  :- Int32
   original-height :- Int32
   format          :- Graphics$TextureImage$TextureFormat
   data            :- ByteBuffer
   mipmap-sizes    :- [Int32]
   mipmap-offsets  :- [Int32]])

(s/defrecord TextureSetAnimationFrame
  [image                :- Image ; TODO: is this necessary?
   vertex-start         :- s/Num
   vertex-count         :- s/Num
   outline-vertex-start :- s/Num
   outline-vertex-count :- s/Num
   tex-coords-start     :- s/Num
   tex-coords-count     :- s/Num])

(s/defrecord TextureSetAnimation
  [id              :- s/Str
   width           :- Int32
   height          :- Int32
   fps             :- Int32
   flip-horizontal :- s/Int
   flip-vertical   :- s/Int
   playback        :- playback-modes
   frames          :- [TextureSetAnimationFrame]])

(s/defrecord TextureSet
  [animations       :- {s/Str TextureSetAnimation}
   vertices         :- s/Any #_editor.gl.vertex/PersistentVertexBuffer
   outline-vertices :- s/Any #_editor.gl.vertex/PersistentVertexBuffer
   tex-coords       :- s/Any #_editor.gl.vertex/PersistentVertexBuffer])

(defprotocol Pass
  (selection?       [this])
  (model-transform? [this]))

(defprotocol Area
  (dimensions [this])
  (empty-space? [this]))

(s/defrecord Region
  [left   :- s/Num
   right  :- s/Num
   top    :- s/Num
   bottom :- s/Num]
  Area
  (dimensions [this] [(- right left) (- bottom top)])
  (empty-space? [this] (or (= 0 (- right left)) (= 0 (- bottom top)))))

(s/defrecord Camera
  [type           :- (s/enum :perspective :orthographic)
   position       :- Point3d
   rotation       :- Quat4d
   z-near         :- s/Num
   z-far          :- s/Num
   fov-x          :- s/Num
   fov-y          :- s/Num
   focus-point    :- Vector4d
   filter-fn      :- Runnable]
  Position
  (position [this] position)
  Rotation
  (rotation [this] rotation))

(g/deftype OutlineCommand
    {:label      (s/maybe s/Str)
     :enabled    (s/maybe  s/Bool)
     :command-fn  (s/maybe  s/Any)
     :context    (s/maybe  s/Any)})

(g/deftype OutlineItem
    {:label    (s/maybe s/Str)
     :icon     (s/maybe Icon)
     :node-ref (s/maybe Long)
     :commands [(s/maybe (:schema @OutlineCommand))]
     :children [(s/maybe s/Any)]})

(defprotocol GeomCloud
  (geom-aabbs [this] [this ids])
  (geom-insert [this positions])
  (geom-delete [this ids])
  (geom-update [this ids f])
  (geom-transform [this ids ^Matrix4d transform]))
