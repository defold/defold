(ns dynamo.buffers
  (:require [schema.macros :as sm])
  (:import [java.nio ByteBuffer ByteOrder]
           [com.google.protobuf ByteString]))

(defn new-buffer [s] (.order (ByteBuffer/allocateDirect s) ByteOrder/LITTLE_ENDIAN))

(defn new-byte-buffer [& dims] (new-buffer (reduce * 1 dims)))

(sm/defn byte-pack :- ByteString
  "Return a byte string from the given byte-buffer."
  ([buffer :- ByteBuffer]
    (ByteString/copyFrom buffer))
  ([buffer :- ByteBuffer & buffers :- [ByteBuffer]]
    (let [cap (reduce + (.capacity buffer) (map #(.capacity %) buffers))
          target (ByteBuffer/allocateDirect cap)]
      (for [buf buffers]
        (.put target buf))
      (ByteString/copyFrom target))))

(defmacro put-rendered-vertex [buf x y z w u v]
  `(do
     (.putFloat ~buf ~x)
     (.putFloat ~buf ~y)
     (.putFloat ~buf ~z)
     (.putFloat ~buf ~w)
     (.putFloat ~buf ~u)
     (.putFloat ~buf ~v)))

(defmacro put-vertex [buf x y z u v]
  `(do
     (.putFloat ~buf ~x)
     (.putFloat ~buf ~y)
     (.putFloat ~buf ~z)
     (.putShort ~buf ~u)
     (.putShort ~buf ~v)))

