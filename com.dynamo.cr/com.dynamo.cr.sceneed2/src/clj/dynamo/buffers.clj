(ns dynamo.buffers
  (:require [schema.core :as s]
            [schema.macros :as sm])
  (:import [java.nio ByteBuffer ByteOrder]
           [com.google.protobuf ByteString]))

(defn slurp-bytes
  [^ByteBuffer buff]
  (let [buff (.duplicate buff)
        n (.remaining buff)
        bytes (byte-array n)]
    (.get buff bytes)
    bytes))

(defn alias-buf-bytes
  "Avoids copy if possible."
  [^ByteBuffer buff]
  (if (and (.hasArray buff) (= (.remaining buff) (alength (.array buff))))
    (.array buff)
    (slurp-bytes buff)))

(defn bbuf->string [^ByteBuffer bb] (String. ^bytes (alias-buf-bytes bb) "UTF-8"))

(defprotocol ByteStringCoding
  (byte-pack [source] "Return a Protocol Buffer compatible byte string from the given source."))

(defn new-buffer [s] (.order (ByteBuffer/allocateDirect s) ByteOrder/LITTLE_ENDIAN))

(defn new-byte-buffer [& dims] (new-buffer (reduce * 1 dims)))

(defn copy-buffer
  [b]
  (let [sz      (.capacity b)
        clone   (if (.isDirect b) (ByteBuffer/allocateDirect sz) (ByteBuffer/allocate sz))
        ro-copy (.asReadOnlyBuffer b)]
    (.flip ro-copy)
    (.put clone ro-copy)))

(defn slice [^ByteBuffer bb offsets]
  (let [dup (.duplicate bb)]
    (doall
      (for [o offsets]
       (do
         (.position dup o)
         (doto (.slice dup)
           (.order (.order bb))))))))

(extend-type ByteBuffer
  ByteStringCoding
  (byte-pack [buffer] (ByteString/copyFrom buffer)))

