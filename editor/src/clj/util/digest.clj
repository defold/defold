;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns util.digest
  (:import [java.io InputStream OutputStream]
           [java.security DigestInputStream DigestOutputStream MessageDigest]
           [org.apache.commons.codec.binary Hex]
           [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(defn bytes->hex
  ^String [^bytes data]
  (Hex/encodeHexString data))

(defn sha1
  ^bytes [^bytes data]
  (DigestUtils/sha1 data))

(defn sha1-hex
  ^String [^bytes data]
  (DigestUtils/sha1Hex data))

(defn string->sha1
  ^bytes [^String s]
  (DigestUtils/sha1 s))

(defn string->sha1-hex
  ^String [^String s]
  (DigestUtils/sha1Hex s))

(defn string->sha256
  ^bytes [^String s]
  (DigestUtils/sha256 s))

(defn string->sha256-hex
  ^String [^String s]
  (DigestUtils/sha256Hex s))

(defn stream->sha1
  ^bytes [^InputStream stream]
  (DigestUtils/sha1 stream))

(defn stream->sha1-hex
  ^String [^InputStream stream]
  (DigestUtils/sha1Hex stream))

(defn stream->sha256
  ^bytes [^InputStream stream]
  (DigestUtils/sha256 stream))

(defn stream->sha256-hex
  ^String [^InputStream stream]
  (DigestUtils/sha256Hex stream))

(defn sha1-hex? [value]
  (and (string? value)
       (= 40 (count value))))

(defn sha256-hex? [value]
  (and (string? value)
       (= 64 (count value))))

(def ^:private ^OutputStream sink-output-stream
  (proxy [OutputStream] []
    (write
      ([byte-or-bytes])
      ([^bytes b, ^long off, ^long len]))))

(defn make-digest-input-stream
  ^DigestInputStream [^InputStream wrapped-stream ^String algorithm]
  (DigestInputStream. wrapped-stream (MessageDigest/getInstance algorithm)))

(defn make-digest-output-stream
  (^DigestOutputStream [^String algorithm]
   (DigestOutputStream. sink-output-stream (MessageDigest/getInstance algorithm)))
  (^DigestOutputStream [^OutputStream wrapped-stream ^String algorithm]
   (DigestOutputStream. wrapped-stream (MessageDigest/getInstance algorithm))))

(defn message-digest->hex
  ^String [^MessageDigest message-digest]
  (-> message-digest .digest bytes->hex))

(defn digest-input-stream->hex
  ^String [^DigestInputStream digest-input-stream]
  (message-digest->hex (.getMessageDigest digest-input-stream)))

(defn completed-stream->hex
  "Given an input or output stream that has completed (i.e. all the data has
  been read or written to it), returns the hex representation of any message
  digest associated with it. If the stream is not a digest input or output
  stream, returns nil."
  ^String [completed-stream]
  (cond
    (instance? DigestOutputStream completed-stream)
    (message-digest->hex (.getMessageDigest ^DigestOutputStream completed-stream))

    (instance? DigestInputStream completed-stream)
    (message-digest->hex (.getMessageDigest ^DigestInputStream completed-stream))))
