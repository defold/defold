(ns util.murmur
  (:import [com.defold.editor.pipeline MurmurHash]))

(defn hash64 [^String v]
  (MurmurHash/hash64 v))

(defn hash64-bytes [^bytes bytes]
  (MurmurHash/hash64 bytes (alength bytes)))
