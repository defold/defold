(ns util.digest
  (:import [java.io InputStream OutputStream]
           [java.security DigestOutputStream MessageDigest]
           [org.apache.commons.codec.digest DigestUtils]
           [org.apache.commons.codec.binary Hex]))

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

(def ^:private ^OutputStream sink-output-stream
  (proxy [OutputStream] []
    (write
      ([byte-or-bytes])
      ([^bytes b, ^long off, ^long len]))))

(defn make-digest-output-stream
  ^DigestOutputStream [^String algorithm]
  (DigestOutputStream. sink-output-stream (MessageDigest/getInstance algorithm)))

(defn bytes->hex [^bytes data]
  (Hex/encodeHexString data))
