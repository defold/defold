(ns dynamo.buffers
  (:require [schema.macros :as sm])
  (:import [java.nio ByteBuffer ByteOrder]
           [com.google.protobuf ByteString]))

(defn new-buffer [s] (.order (ByteBuffer/allocateDirect s) ByteOrder/LITTLE_ENDIAN))

(defn new-byte-buffer [& dims] (new-buffer (reduce * 1 dims)))


(sm/defn byte-pack :- ByteString
  "Return a byte string from the given byte-buffer."
  [buffer :- ByteBuffer]
  (ByteString/copyFrom buffer))

