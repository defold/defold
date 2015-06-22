(ns dynamo.types
  "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
  (:require [potemkin.namespaces :refer [import-vars]]
            [schema.core :as s])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.tile.proto Tile$Playback]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]))

(import-vars [schema.core Any Bool Inst Int Keyword Num Regex Schema Str Symbol Uuid check enum protocol maybe fn-schema one optional-key pred recursive required-key validate])

; ----------------------------------------
; Protocols here help avoid circular dependencies
; ----------------------------------------
(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(defn disposable? [x] (satisfies? IDisposable x))

(defprotocol Condition
  (signal [this] "Notify a deferred action of something"))

(defprotocol Cancelable
  (cancel [this] "Cancel a thing."))

(defprotocol FileContainer
  (node-for-path [this path] "Create a new node from a path within the container. `path` must be a ProjectPath."))

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

; ----------------------------------------
; Functions to create basic value types
; ----------------------------------------
(defprotocol PropertyType
  (property-value-type    [this]   "Prismatic schema for property value type")
  (property-default-value [this])
  (property-validate      [this v] "Returns a possibly-empty seq of messages.")
  (property-valid-value?  [this v] "If valid, returns nil. If invalid, returns seq of Marker")
  (property-enabled?      [this v] "If true, this property may be edited")
  (property-visible?      [this v] "If true, this property appears in the UI")
  (property-tags          [this]))

(defn property-type? [x] (satisfies? PropertyType x))

(def Properties {Keyword {:value Any :type (protocol PropertyType)}})

(def Int32   (s/both s/Int (pred #(< Integer/MIN_VALUE % Integer/MAX_VALUE) 'int32?)))

(def Icon    Str)

(def Color   [Num])

(def Vec3    [(one Num "x")
              (one Num "y")
              (one Num "z")])

(defn Point3d->Vec3 [^Point3d p]
  [(.getX p) (.getY p) (.getZ p)])

(def MouseType (enum :one-button :three-button))

(def Registry {Any Any})

(s/defrecord Rect
  [path     :- Any
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
  ([x :- Num y :- Num width :- Num height :- Num]
    (rect "" (int  x) (int y) (int width) (int height)))
  ([path :- Any x :- Num y :- Num width :- Num height :- Num]
    (Rect. path (int x) (int y) (int width) (int height))))

(s/defrecord Image
  [path     :- Any
   contents :- BufferedImage
   width    :- Int32
   height   :- Int32]
  ImageHolder
  (contents [this] contents))

(def AnimationPlayback (enum :PLAYBACK_NONE :PLAYBACK_ONCE_FORWARD :PLAYBACK_ONCE_BACKWARD
                             :PLAYBACK_ONCE_PINGPONG :PLAYBACK_LOOP_FORWARD :PLAYBACK_LOOP_BACKWARD
                             :PLAYBACK_LOOP_PINGPONG))

(s/defrecord Animation
  [id              :- Str
   images          :- [Image]
   fps             :- Int32
   flip-horizontal :- Int
   flip-vertical   :- Int
   playback        :- AnimationPlayback])

(s/defrecord TexturePacking
  [aabb         :- Rect
   packed-image :- BufferedImage
   coords       :- [Rect]
   sources      :- [Rect]
   animations   :- [Animation]])

(s/defrecord Vertices
  [counts   :- [Int32]
   starts   :- [Int32]
   vertices :- [Num]])

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
   vertex-start         :- Num
   vertex-count         :- Num
   outline-vertex-start :- Num
   outline-vertex-count :- Num
   tex-coords-start     :- Num
   tex-coords-count     :- Num])

(s/defrecord TextureSetAnimation
  [id              :- Str
   width           :- Int32
   height          :- Int32
   fps             :- Int32
   flip-horizontal :- Int
   flip-vertical   :- Int
   playback        :- AnimationPlayback
   frames          :- [TextureSetAnimationFrame]])

(s/defrecord TextureSet
  [animations       :- {Str TextureSetAnimation}
   vertices         :- Any #_editor.gl.vertex/PersistentVertexBuffer
   outline-vertices :- Any #_editor.gl.vertex/PersistentVertexBuffer
   tex-coords       :- Any #_editor.gl.vertex/PersistentVertexBuffer])

(defprotocol Pass
  (selection?       [this])
  (model-transform? [this]))

(s/defrecord Region
  [left   :- Num
   right  :- Num
   top    :- Num
   bottom :- Num])

(defprotocol Viewport
  (viewport ^Region [this]))

(s/defrecord Camera
  [type           :- (enum :perspective :orthographic)
   position       :- Point3d
   rotation       :- Quat4d
   z-near         :- Num
   z-far          :- Num
   aspect         :- Num
   fov            :- Num
   focus-point    :- Vector4d]
  Position
  (position [this] position)
  Rotation
  (rotation [this] rotation))

(def OutlineCommand
  {:label      Str
   :enabled    Bool
   :command-fn Any
   :context    Any})

(def OutlineItem
  {:label    Str
   :icon     Icon
   :node-ref Long
   :commands [OutlineCommand]
   :children [(recursive #'OutlineItem)]})
