(ns util.digest
  (:import [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(defn sha1 [^bytes data]
  (DigestUtils/sha1 data))

(defn sha1->hex [^bytes data]
  (DigestUtils/sha1Hex data))