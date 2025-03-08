;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.buffers-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as b]
            [support.test-support :refer [array=]])
  (:import [java.nio ByteBuffer]))

(defn- buffer-with-contents ^ByteBuffer [byte-values]
  (doto (b/new-byte-buffer (count byte-values))
    (.put (byte-array byte-values))
    .rewind))

(defn- buffer-properties [^ByteBuffer buffer]
  [(.position buffer)
   (.limit    buffer)
   (.capacity buffer)])

(deftest slurp-bytes
  (testing "returns remaining bytes"
    (are [expected buffer] (array= (byte-array expected) (b/slurp-bytes buffer))
      []        (doto (buffer-with-contents []))
      [1 2 3 4] (doto (buffer-with-contents [1 2 3 4]))
      [2 3 4]   (doto (buffer-with-contents [1 2 3 4]) .get)))
  (testing "does not impact input buffer properties"
    (let [b (buffer-with-contents [1 2 3 4])]
      (is (= [0 4 4] (buffer-properties b)))
      (b/slurp-bytes b)
      (is (= [0 4 4] (buffer-properties b)))
      (.get b)
      (is (= [1 4 4] (buffer-properties b)))
      (b/slurp-bytes b)
      (is (= [1 4 4] (buffer-properties b))))))

(deftest alias-buf-bytes
  (let [arr (byte-array [1 2 3 4])]
    (testing "returns remaining bytes"
      (are [expected buffer] (array= (byte-array expected) (b/alias-buf-bytes buffer))
        []        (doto (buffer-with-contents []))
        [1 2 3 4] (doto (buffer-with-contents [1 2 3 4]))
        [2 3 4]   (doto (buffer-with-contents [1 2 3 4]) .get)
        [1 2 3 4] (doto (ByteBuffer/wrap arr))
        [1 2 3 4] (doto (ByteBuffer/wrap arr 0 4))
        [2 3 4]   (doto (ByteBuffer/wrap arr 0 4) .get)
        [1 2]     (doto (ByteBuffer/wrap arr 0 2))
        [2 3]     (doto (ByteBuffer/wrap arr 1 2))
        [3]       (doto (ByteBuffer/wrap arr 1 2) .get)))
    (testing "returns wrapped array when possible"
      (are [buffer] (identical? arr (b/alias-buf-bytes buffer))
        (ByteBuffer/wrap arr)
        (ByteBuffer/wrap arr 0 4))
      (are [buffer] (not (identical? arr (b/alias-buf-bytes buffer)))
        (ByteBuffer/wrap arr 1 2)
        (doto (ByteBuffer/wrap arr) .get)
        (doto (ByteBuffer/wrap arr 0 4) .get)))
    (testing "does not impact input buffer properties"
      (let [b (ByteBuffer/wrap arr)]
        (is (= [0 4 4] (buffer-properties b)))
        (b/alias-buf-bytes b)
        (is (= [0 4 4] (buffer-properties b)))
        (.get b)
        (is (= [1 4 4] (buffer-properties b)))
        (b/alias-buf-bytes b)
        (is (= [1 4 4] (buffer-properties b)))))))

(deftest bbuf->string
  (let [bytes [0x55 0x72 0x73 0xc3 0xa4 0x6b 0x74 0x61 0x20 0x6d 0x69 0x67]]
    (testing "returns buffer contents interpreted as a UTF-8-encoded string"
      (are [expected buffer] (= expected (b/bbuf->string buffer))
        "Ursäkta mig" (ByteBuffer/wrap (byte-array bytes))
        "Ursäkta mig" (buffer-with-contents bytes)
         "rsäkta mig" (doto (buffer-with-contents bytes) .get)
          "säkta mig" (doto (buffer-with-contents bytes) .get .get)
           "äkta mig" (doto (buffer-with-contents bytes) .get .get .get)
            "kta mig" (doto (buffer-with-contents bytes) .get .get .get .get .get)))
    (testing "does not impact input buffer properties"
      (let [b (buffer-with-contents bytes)]
        (is (= [0 12 12] (buffer-properties b)))
        (b/bbuf->string b)
        (is (= [0 12 12] (buffer-properties b)))
        (.get b)
        (is (= [1 12 12] (buffer-properties b)))
        (b/bbuf->string b)
        (is (= [1 12 12] (buffer-properties b)))))))

(defn- contents-of
  [^ByteBuffer buffer]
  (let [arr (byte-array (.limit buffer))]
    (doto (.asReadOnlyBuffer buffer)
      .rewind
      (.get arr))
    arr))

(deftest copy-buffer
  (testing "returns a buffer containing contents before current position"
    (are [expected buffer] (array= (byte-array expected) (contents-of (b/copy-buffer buffer)))
      []        (buffer-with-contents [])
      [0 0 0 0] (buffer-with-contents [1 2 3 4])
      [1 2]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)))
      [1 2 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)))
      [0 0]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [0 0 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 0]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 0 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [0 0]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip)
      [0 0 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 0]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      [1 0 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip .get)))
  (testing "does not impact input buffer properties"
    (let [b (buffer-with-contents [1 2 3 4])]
      (is (= [0 4 4] (buffer-properties b)))
      (b/copy-buffer b)
      (is (= [0 4 4] (buffer-properties b)))
      (.get b)
      (is (= [1 4 4] (buffer-properties b)))
      (b/copy-buffer b)
      (is (= [1 4 4] (buffer-properties b)))))
  (testing "output properties match input properties"
    (are [expected input]
      (let [input-buffer input
            copy1        (b/copy-buffer input-buffer)
            copy2        (b/copy-buffer copy1)]
        (= expected (buffer-properties input-buffer) (buffer-properties copy1) (buffer-properties copy2)))
      [0 0 0] (buffer-with-contents [])
      [0 4 4] (buffer-with-contents [1 2 3 4])
      [2 2 2] (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)))
      [2 4 4] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)))
      [0 2 2] (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [0 4 4] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 2 2] (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 4 4] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [0 2 2] (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip)
      ; TODO: output limit does not match input
      ;[0 2 4] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 2 2] (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      ; TODO: output limit does not match input
      ;[1 2 4] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip .get)
      ))
  (testing "input and output buffer data is independent"
    (let [in  (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)))
          out (b/copy-buffer in)]
      (.put in  (byte 3))
      (.put out (byte 4))
      (is (= [1 2 3 0] (seq (contents-of in))))
      (is (= [1 2 4 0] (seq (contents-of out)))))))

(deftest byte-pack
  (testing "returns a ByteString containing contents before current position"
    (are [expected buffer] (array= (byte-array expected) (.toByteArray (b/byte-pack buffer)))
      []        (buffer-with-contents [])
      [1 2 3 4] (buffer-with-contents [1 2 3 4])
      []        (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)))
      [0 0]     (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)))
      [1 2]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 2 0 0] (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [2]       (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [2 0 0]   (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 2]     (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 2]     (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [2]       (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      [2]       (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip .get)))
  (testing "multiple calls to byte-pack return the same value"
    (are [buffer] (let [buffer-val   buffer
                        byte-string1 (b/byte-pack buffer-val)
                        byte-string2 (b/byte-pack buffer-val)]
                    (array= (.toByteArray byte-string1) (.toByteArray byte-string2)))
      (buffer-with-contents [])
      (buffer-with-contents [1 2 3 4])
      (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)))
      (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)))
      (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip)
      (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip)
      (doto (b/new-byte-buffer 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      (doto (b/new-byte-buffer 4) (.put (byte 1)) (.put (byte 2)) .flip .get))))

;; -----------------------------------------------------------------------------
;; Buffer push! and put! tests
;; -----------------------------------------------------------------------------

(defn- element-at-offset-fn [data-type]
  (case data-type
    (:double) #(.getDouble ^ByteBuffer %1 ^long %2)
    (:float) #(.getFloat ^ByteBuffer %1 ^long %2)
    (:int :uint) #(.getInt ^ByteBuffer %1 ^long %2)
    (:short :ushort) #(.getShort ^ByteBuffer %1 ^long %2)
    (:byte :ubyte) #(.get ^ByteBuffer %1 ^long %2)))

(defn- buffer->data-fn [data-type]
  (let [primitive-type-kw (b/primitive-type-kw data-type)
        element-byte-size (b/type-size data-type)
        element-at-offset-fn (element-at-offset-fn data-type)]
    (fn buffer->data [^ByteBuffer buffer]
      (into (vector-of primitive-type-kw)
            (map #(element-at-offset-fn buffer %))
            (range 0 (.capacity buffer) element-byte-size)))))

(defn- buffer->bytes [^ByteBuffer buffer]
  (into (vector-of :byte)
        (map #(.get buffer ^long %))
        (range 0 (.capacity buffer) Byte/BYTES)))

(defn- out-data-fn [data-type]
  (partial vector-of (b/primitive-type-kw data-type)))

(defn- limits [data-type normalize]
  (let [out-min
        (case data-type
          :double Double/MIN_VALUE
          :float Float/MIN_VALUE
          :int Integer/MIN_VALUE
          :uint (int 0)
          :short Short/MIN_VALUE
          :ushort (short 0)
          :byte Byte/MIN_VALUE
          :ubyte (byte 0))

        in-min
        (case data-type
          (:double :float) out-min
          (:int :short :byte) (if normalize -1.0 out-min)
          (:uint :ushort :ubyte) (if normalize 0.0 out-min))

        out-max
        (case data-type
          :double Double/MAX_VALUE
          :float Float/MAX_VALUE
          :int Integer/MAX_VALUE
          :uint (unchecked-int 0xffffffff)
          :short Short/MAX_VALUE
          :ushort (unchecked-short 0xffff)
          :byte Byte/MAX_VALUE
          :ubyte (unchecked-byte 0xff))

        in-max
        (if normalize
          (case data-type
            (:double :float) out-max
            (:int :uint :short :ushort :byte :ubyte) 1.0)
          (case data-type
            :double Double/MAX_VALUE
            :float Float/MAX_VALUE
            :int (long Integer/MAX_VALUE)
            :uint (long 0xffffffff)
            :short (long Short/MAX_VALUE)
            :ushort (long 0xffff)
            :byte (long Byte/MAX_VALUE)
            :ubyte (long 0xff)))]

    [in-min in-max out-min out-max]))

(defn- make-buffer
  ^ByteBuffer [byte-size]
  (b/little-endian (b/new-byte-buffer byte-size)))

(deftest blit!-test
  (let [min Byte/MIN_VALUE
        max Byte/MAX_VALUE
        buffer->data (buffer->data-fn :byte)
        out-data (out-data-fn :byte)
        in-bytes (comp byte-array out-data)
        buf (make-buffer 4)]
    (is (identical? buf (b/blit! buf 0 (in-bytes min min min min))) "Returns self")
    (is (zero? (.position buf)) "Position is unaffected")
    (is (= (out-data min min min min) (buffer->data buf)))
    (b/blit! buf 1 (in-bytes max max))
    (is (= (out-data min max max min) (buffer->data buf)))))

(deftest put-floats!-test
  (let [min Float/MIN_VALUE
        max Float/MAX_VALUE
        element-byte-size (b/type-size :float)
        buffer->data (buffer->data-fn :float)
        out-data (out-data-fn :float)
        buf (make-buffer (* 4 element-byte-size))]
    (is (identical? buf (b/put-floats! buf 0 (vector min min min min))) "Put returns self")
    (is (zero? (.position buf)) "Position is unaffected")
    (is (= (out-data min min min min) (buffer->data buf)))
    (b/put-floats! buf element-byte-size (vector max max))
    (is (= (out-data min max max min) (buffer->data buf)))))

(deftest put!-test
  (doseq [normalize [false true]
          data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
    (let [[in-min in-max out-min out-max] (limits data-type normalize)
          element-byte-size (b/type-size data-type)
          buffer->data (buffer->data-fn data-type)
          out-data (out-data-fn data-type)
          buf (make-buffer (* 4 element-byte-size))]
      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name data-type))
        (is (identical? buf (b/put! buf 0 data-type normalize (vector in-min in-min in-min in-min))) "Returns self")
        (is (zero? (.position buf)) "Position is unaffected")
        (is (= (out-data out-min out-min out-min out-min) (buffer->data buf)))
        (b/put! buf element-byte-size data-type normalize (vector in-max in-max))
        (is (= (out-data out-min out-max out-max out-min) (buffer->data buf)))))))

(deftest push-floats!-test
  (let [min Float/MIN_VALUE
        max Float/MAX_VALUE
        element-byte-size (b/type-size :float)
        buffer->data (buffer->data-fn :float)
        out-data (out-data-fn :float)
        buf (make-buffer (* 4 element-byte-size))]
    (is (identical? buf (b/push-floats! buf (vector min min))) "Returns self")
    (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
    (is (= (out-data min min 0 0) (buffer->data buf)))
    (b/push-floats! buf (vector max max))
    (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
    (is (= (out-data min min max max) (buffer->data buf)))))

(deftest push!-test
  (doseq [normalize [false true]
          data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
    (let [[in-min in-max out-min out-max] (limits data-type normalize)
          element-byte-size (b/type-size data-type)
          buffer->data (buffer->data-fn data-type)
          out-data (out-data-fn data-type)
          buf (make-buffer (* 4 element-byte-size))]
      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name data-type))
        (is (identical? buf (b/push! buf data-type normalize (vector in-min in-min))) "Returns self")
        (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
        (is (= (out-data out-min out-min 0 0) (buffer->data buf)))
        (b/push! buf data-type normalize (vector in-max in-max))
        (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
        (is (= (out-data out-min out-min out-max out-max) (buffer->data buf)))))))
