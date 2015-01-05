(ns dynamo.types
  (:require [schema.core :as s]
            [schema.macros :as sm])
  (:import [java.awt.image BufferedImage]
           [java.nio ByteBuffer]
           [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.tile.proto Tile$Playback]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

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

(defprotocol NamingContext
  (lookup [this nm] "Locate a value by name"))

(defprotocol Node
  (properties [this]          "Produce a description of properties supported by this node.")
  (inputs     [this]          "Return a set of labels for the allowed inputs of the node.")
  (outputs    [this]          "Return a set of labels for the outputs of this node.")
  (auto-update? [this label]  "Return true if the output label should be updated whenever it gets invalidated.")
  (cached-outputs [this]      "Return a set of labels for the outputs of this node which are cached. This must be a subset of 'outputs'.")
  (output-dependencies [this] "Return a map of labels for the inputs and properties to outputs that depend on them."))

(defprotocol MessageTarget
  (process-one-event [this event]))

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

; ----------------------------------------
; Functions to create basic value types
; ----------------------------------------
(defn apply-if-fn [f & args]
  (if (fn? f)
    (apply f args)
    f))

(defn var-get-recursive [var-or-value]
  (if (var? var-or-value)
    (recur (var-get var-or-value))
    var-or-value))

(defprotocol PropertyType
  (property-value-type    [this] "Prismatic schema for property value type")
  (default-property-value [this])
  (valid-property-value?  [this v])
  (property-visible       [this] "If true, this property appears in the UI"))

(defn property-type? [x] (satisfies? PropertyType x))

(def Properties {s/Keyword {:value s/Any :type (s/protocol PropertyType)}})

(def Int32   (s/pred #(instance? java.lang.Integer %) 'int32?))
(def Icon    s/Str)
(def NodeRef s/Int)
(def Color   [s/Num])
(def Vec3    [(s/one s/Num "x")
              (s/one s/Num "y")
              (s/one s/Num "z")])

(def MouseType (s/enum :one-button :three-button))

(def Callbacks [clojure.lang.IFn])

(def Registry {s/Any s/Any})

(sm/defrecord Rect
  [path     :- s/Any
   x        :- Int32
   y        :- Int32
   width    :- Int32
   height   :- Int32]
  N2Extent
  (width [this] width)
  (height [this] height))

(sm/defrecord AABB [min max]
  R3Min
  (min-p [this] (.min this))
  R3Max
  (max-p [this] (.max this)))

(defmethod print-method AABB
  [^AABB v ^java.io.Writer w]
  (.write w (str "<AABB \"min: " (.min v) ", max: " (.max v) "\">")))

(sm/defn ^:always-validate rect :- Rect
  ([x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (rect "" (int  x) (int y) (int width) (int height)))
  ([path :- s/Any x :- s/Num y :- s/Num width :- s/Num height :- s/Num]
    (Rect. path (int x) (int y) (int width) (int height))))

(sm/defrecord Image
  [path     :- s/Any
   contents :- BufferedImage
   width    :- Int32
   height   :- Int32]
  ImageHolder
  (contents [this] contents))

(def AnimationPlayback (s/enum :PLAYBACK_NONE :PLAYBACK_ONCE_FORWARD :PLAYBACK_ONCE_BACKWARD
                               :PLAYBACK_ONCE_PINGPONG :PLAYBACK_LOOP_FORWARD :PLAYBACK_LOOP_BACKWARD
                               :PLAYBACK_LOOP_PINGPONG))

(sm/defrecord Animation
  [id              :- s/Str
   images          :- [Image]
   fps             :- Int32
   flip-horizontal :- s/Int
   flip-vertical   :- s/Int
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

(defprotocol Pass
  (selection?       [this])
  (model-transform? [this]))

(def RenderData {(s/required-key Pass) s/Any})

(sm/defrecord Region
  [left   :- s/Num
   right  :- s/Num
   top    :- s/Num
   bottom :- s/Num])

(defprotocol Viewport
  (viewport ^Region [this]))

(sm/defrecord Camera
  [type           :- (s/enum :perspective :orthographic)
   position       :- Point3d
   rotation       :- Quat4d
   z-near         :- s/Num
   z-far          :- s/Num
   aspect         :- s/Num
   fov            :- s/Num
   focus-point    :- Vector4d
   viewport       :- Region]
  Position
  (position [this] position)
  Rotation
  (rotation [this] rotation)
  Viewport
  (viewport [this] viewport))

(def OutlineItem
  {:label    s/Str
   :icon     Icon
   :node-ref NodeRef
   :children [(s/recursive #'OutlineItem)]})

; ----------------------------------------
; Type compatibility and inference
; ----------------------------------------
(defn- check-single-type
  [out in]
  (or
   (= s/Any in)
   (= out in)
   (and (class? in) (class? out) (.isAssignableFrom ^Class in out))))

(defn compatible?
  [output-schema input-schema expect-collection?]
  (let [out-t-pl? (coll? output-schema)
        in-t-pl?  (coll? input-schema)]
    (or
     (= s/Any input-schema)
     (and expect-collection? (= [s/Any] input-schema))
     (and expect-collection? in-t-pl? (check-single-type output-schema (first input-schema)))
     (and (not expect-collection?) (check-single-type output-schema input-schema))
     (and (not expect-collection?) in-t-pl? out-t-pl? (check-single-type (first output-schema) (first input-schema))))))

(doseq [[v doc]
       {*ns*                   "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
        #'Icon                 "*schema* - schema for the representation of an Icon as s/Str"
        #'NodeRef              "*schema* - schema for the representation of a node reference as s/Int"
        #'Pass                 "value for a rendering pass"
        #'selection?           "Replies true when the pass is used during pick render."
        #'model-transform?     "Replies true when the pass should apply the node transforms to the current model-view matrix. (Will be true in most cases, false for overlays.)"}]
  (alter-meta! v assoc :doc doc))
