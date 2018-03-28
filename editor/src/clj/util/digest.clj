(ns util.digest
  (:import [java.io InputStream]
           [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(defn sha1 [^bytes data]
  (DigestUtils/sha1 data))

(defn sha1-hex [^bytes data]
  (DigestUtils/sha1Hex data))

(defn string->sha1 [^String s]
  (DigestUtils/sha1 s))

(defn string->sha1-hex [^String s]
  (DigestUtils/sha1Hex s))

(defn stream->sha1 [^InputStream stream]
  (DigestUtils/sha1 stream))

(defn stream->sha1-hex [^InputStream stream]
  (DigestUtils/sha1Hex stream))

