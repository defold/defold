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

(ns editor.buffers-test
  (:require [clojure.test :refer :all]
            [editor.buffers :as b]
            [support.test-support :refer [array=]]
            [util.num :as num])
  (:import [com.google.protobuf ByteString]
           [java.nio ByteBuffer CharBuffer DoubleBuffer FloatBuffer IntBuffer LongBuffer ShortBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- buffer-with-byte-size
  (^ByteBuffer [byte-size]
   (buffer-with-byte-size byte-size :byte-order/native))
  (^ByteBuffer [byte-size byte-order]
   (b/new-byte-buffer byte-size byte-order)))

(defn- buffer-with-contents
  ^ByteBuffer [byte-values]
  (doto (buffer-with-byte-size (count byte-values))
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
      [1 2]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)))
      [1 2 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)))
      [0 0]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [0 0 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 0]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 0 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [0 0]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip)
      [0 0 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 0]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      [1 0 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip .get)))
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
      [2 2 2] (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)))
      [2 4 4] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)))
      [0 2 2] (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [0 4 4] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 2 2] (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 4 4] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [0 2 2] (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip)
      ; TODO: output limit does not match input
      ;[0 2 4] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 2 2] (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      ; TODO: output limit does not match input
      ;[1 2 4] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip .get)
      ))
  (testing "input and output buffer data is independent"
    (let [in  (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)))
          out (b/copy-buffer in)]
      (.put in  (byte 3))
      (.put out (byte 4))
      (is (= [1 2 3 0] (seq (contents-of in))))
      (is (= [1 2 4 0] (seq (contents-of out)))))))

(deftest byte-pack
  (testing "returns a ByteString containing contents before current position"
    (are [expected buffer] (array= (byte-array expected) (.toByteArray ^ByteString (b/byte-pack buffer)))
      []        (buffer-with-contents [])
      [1 2 3 4] (buffer-with-contents [1 2 3 4])
      []        (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)))
      [0 0]     (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)))
      [1 2]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      [1 2 0 0] (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      [2]       (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [2 0 0]   (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      [1 2]     (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip)
      [1 2]     (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip)
      [2]       (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      [2]       (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip .get)))
  (testing "multiple calls to byte-pack return the same value"
    (are [buffer] (let [buffer-val buffer
                        ^ByteString byte-string1 (b/byte-pack buffer-val)
                        ^ByteString byte-string2 (b/byte-pack buffer-val)]
                    (array= (.toByteArray byte-string1) (.toByteArray byte-string2)))
      (buffer-with-contents [])
      (buffer-with-contents [1 2 3 4])
      (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)))
      (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)))
      (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind)
      (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind)
      (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .rewind .get)
      (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip)
      (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip)
      (doto (buffer-with-byte-size 2) (.put (byte 1)) (.put (byte 2)) .flip .get)
      (doto (buffer-with-byte-size 4) (.put (byte 1)) (.put (byte 2)) .flip .get))))

(def ^:private typed-buffer-round-trip-checks
  [{:label 'ByteBuffer
    :make #(.put (buffer-with-byte-size Byte/BYTES %) 0 Byte/MAX_VALUE)
    :get #(.get ^ByteBuffer % 0)}
   {:label 'CharBuffer
    :make #(.put (.asCharBuffer (buffer-with-byte-size Character/BYTES %)) 0 Character/MAX_VALUE)
    :get #(.get ^CharBuffer % 0)}
   {:label 'DoubleBuffer
    :make #(.put (.asDoubleBuffer (buffer-with-byte-size Double/BYTES %)) 0 Double/MAX_VALUE)
    :get #(.get ^DoubleBuffer % 0)}
   {:label 'FloatBuffer
    :make #(.put (.asFloatBuffer (buffer-with-byte-size Float/BYTES %)) 0 Float/MAX_VALUE)
    :get #(.get ^FloatBuffer % 0)}
   {:label 'IntBuffer
    :make #(.put (.asIntBuffer (buffer-with-byte-size Integer/BYTES %)) 0 Integer/MAX_VALUE)
    :get #(.get ^IntBuffer % 0)}
   {:label 'LongBuffer
    :make #(.put (.asLongBuffer (buffer-with-byte-size Long/BYTES %)) 0 Long/MAX_VALUE)
    :get #(.get ^LongBuffer % 0)}
   {:label 'ShortBuffer
    :make #(.put (.asShortBuffer (buffer-with-byte-size Short/BYTES %)) 0 Short/MAX_VALUE)
    :get #(.get ^ShortBuffer % 0)}])

(def ^:private byte-buffer-multi-byte-round-trip-checks
  [{:label 'ByteBuffer/.putChar
    :make #(.putChar (buffer-with-byte-size Character/BYTES %) 0 Character/MAX_VALUE)
    :get #(.getChar ^ByteBuffer % 0)}
   {:label 'ByteBuffer/.putDouble
    :make #(.putDouble (buffer-with-byte-size Double/BYTES %) 0 Double/MAX_VALUE)
    :get #(.getDouble ^ByteBuffer % 0)}
   {:label 'ByteBuffer/.putFloat
    :make #(.putFloat (buffer-with-byte-size Float/BYTES %) 0 Float/MAX_VALUE)
    :get #(.getFloat ^ByteBuffer % 0)}
   {:label 'ByteBuffer/.putInt
    :make #(.putInt (buffer-with-byte-size Integer/BYTES %) 0 Integer/MAX_VALUE)
    :get #(.getInt ^ByteBuffer % 0)}
   {:label 'ByteBuffer/.putLong
    :make #(.putLong (buffer-with-byte-size Long/BYTES %) 0 Long/MAX_VALUE)
    :get #(.getLong ^ByteBuffer % 0)}
   {:label 'ByteBuffer/.putShort
    :make #(.putShort (buffer-with-byte-size Short/BYTES %) 0 Short/MAX_VALUE)
    :get #(.getShort ^ByteBuffer % 0)}])

(defn round-trip-check! [round-trip-checks check-fn!]
  (doseq [byte-order [:byte-order/big-endian :byte-order/little-endian :byte-order/native]
          {:keys [label make] :as round-trip-check} round-trip-checks]
    (testing (str label " " byte-order)
      (let [buffer (make byte-order)]
        (check-fn! buffer round-trip-check)))))

(defn- first-reduced [reducible]
  (reduce #(reduced %2) nil reducible))

(deftest read-only-byte-order-test
  (round-trip-check!
    (concat
      typed-buffer-round-trip-checks
      byte-buffer-multi-byte-round-trip-checks)
    (fn [buffer {:keys [get]}]
      (is (= (get buffer)
             (get (b/read-only buffer)))))))

(deftest reducible-byte-order-test
  (round-trip-check!
    typed-buffer-round-trip-checks
    (fn [buffer {:keys [get]}]
      (is (= (get buffer)
             (first-reduced (b/reducible buffer)))))))

(deftest buffer-data-reduce-byte-order-test
  (round-trip-check!
    typed-buffer-round-trip-checks
    (fn [buffer {:keys [get]}]
      (is (= (get buffer)
             (first-reduced (b/make-buffer-data buffer))))))
  (round-trip-check!
    byte-buffer-multi-byte-round-trip-checks
    (fn [^ByteBuffer buffer _]
      (let [bytes (byte-array (b/item-count buffer))]
        (.get buffer 0 bytes)
        (is (= (into (vector-of :byte)
                     bytes)
               (into (vector-of :byte)
                     (b/make-buffer-data buffer))))))))

(deftest buffer-data-type?-test
  (is (every? false? (map b/buffer-data-type? [nil 1 "string" :keyword (Object.) #{}])))
  (is (every? true? (map b/buffer-data-type? b/buffer-data-types))))

;; -----------------------------------------------------------------------------
;; Buffer push! and put! tests
;; -----------------------------------------------------------------------------

(defn- element-at-offset-fn [buffer-data-type]
  (case buffer-data-type
    (:double) #(.getDouble ^ByteBuffer %1 ^long %2)
    (:float) #(.getFloat ^ByteBuffer %1 ^long %2)
    (:int :uint) #(.getInt ^ByteBuffer %1 ^long %2)
    (:short :ushort) #(.getShort ^ByteBuffer %1 ^long %2)
    (:byte :ubyte) #(.get ^ByteBuffer %1 ^long %2)))

(defn- buffer->data-fn [buffer-data-type]
  (let [primitive-type-kw (b/primitive-type-kw buffer-data-type)
        element-byte-size (b/type-size buffer-data-type)
        element-at-offset-fn (element-at-offset-fn buffer-data-type)]
    (fn buffer->data [^ByteBuffer buffer]
      (into (vector-of primitive-type-kw)
            (map #(element-at-offset-fn buffer %))
            (range 0 (.capacity buffer) element-byte-size)))))

(defn- buffer->bytes [^ByteBuffer buffer]
  (into (vector-of :byte)
        (map #(.get buffer ^long %))
        (range 0 (.capacity buffer) Byte/BYTES)))

(defn- out-data-fn [buffer-data-type]
  (partial vector-of (b/primitive-type-kw buffer-data-type)))

(defn- limits [buffer-data-type normalize]
  (let [out-min
        (case buffer-data-type
          :double Double/MIN_VALUE
          :float Float/MIN_VALUE
          :int Integer/MIN_VALUE
          :uint (int 0)
          :short Short/MIN_VALUE
          :ushort (short 0)
          :byte Byte/MIN_VALUE
          :ubyte (byte 0))

        in-min
        (case buffer-data-type
          (:double :float) out-min
          (:int :short :byte) (if normalize -1.0 out-min)
          (:uint :ushort :ubyte) (if normalize 0.0 out-min))

        out-max
        (case buffer-data-type
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
          (case buffer-data-type
            (:double :float) out-max
            (:int :uint :short :ushort :byte :ubyte) 1.0)
          (case buffer-data-type
            :double Double/MAX_VALUE
            :float Float/MAX_VALUE
            :int (long Integer/MAX_VALUE)
            :uint (long 0xffffffff)
            :short (long Short/MAX_VALUE)
            :ushort (long 0xffff)
            :byte (long Byte/MAX_VALUE)
            :ubyte (long 0xff)))]

    [in-min in-max out-min out-max]))

(deftest blit!-test
  (let [min Byte/MIN_VALUE
        max Byte/MAX_VALUE
        buffer->data (buffer->data-fn :byte)
        out-data (out-data-fn :byte)
        in-bytes (comp byte-array out-data)
        buf (buffer-with-byte-size 4)]
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
        buf (buffer-with-byte-size (* 4 element-byte-size))]
    (is (identical? buf (b/put-floats! buf 0 (vector min min min min))) "Put returns self")
    (is (zero? (.position buf)) "Position is unaffected")
    (is (= (out-data min min min min) (buffer->data buf)))
    (b/put-floats! buf element-byte-size (vector max max))
    (is (= (out-data min max max min) (buffer->data buf)))))

(deftest put!-test
  (doseq [normalize [false true]
          buffer-data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
    (let [[in-min in-max out-min out-max] (limits buffer-data-type normalize)
          element-byte-size (b/type-size buffer-data-type)
          buffer->data (buffer->data-fn buffer-data-type)
          out-data (out-data-fn buffer-data-type)
          buf (buffer-with-byte-size (* 4 element-byte-size))]
      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name buffer-data-type))
        (is (identical? buf (b/put! buf 0 buffer-data-type normalize (vector in-min in-min in-min in-min))) "Returns self")
        (is (zero? (.position buf)) "Position is unaffected")
        (is (= (out-data out-min out-min out-min out-min) (buffer->data buf)))
        (b/put! buf element-byte-size buffer-data-type normalize (vector in-max in-max))
        (is (= (out-data out-min out-max out-max out-min) (buffer->data buf)))))))

(deftest push-floats!-test
  (let [min Float/MIN_VALUE
        max Float/MAX_VALUE
        element-byte-size (b/type-size :float)
        buffer->data (buffer->data-fn :float)
        out-data (out-data-fn :float)
        buf (buffer-with-byte-size (* 4 element-byte-size))]
    (is (identical? buf (b/push-floats! buf (vector min min))) "Returns self")
    (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
    (is (= (out-data min min 0 0) (buffer->data buf)))
    (b/push-floats! buf (vector max max))
    (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
    (is (= (out-data min min max max) (buffer->data buf)))))

(deftest push!-test
  (doseq [normalize [false true]
          buffer-data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
    (let [[in-min in-max out-min out-max] (limits buffer-data-type normalize)
          element-byte-size (b/type-size buffer-data-type)
          buffer->data (buffer->data-fn buffer-data-type)
          out-data (out-data-fn buffer-data-type)
          buf (buffer-with-byte-size (* 4 element-byte-size))]
      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name buffer-data-type))
        (is (identical? buf (b/push! buf buffer-data-type normalize (vector in-min in-min))) "Returns self")
        (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
        (is (= (out-data out-min out-min 0 0) (buffer->data buf)))
        (b/push! buf buffer-data-type normalize (vector in-max in-max))
        (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
        (is (= (out-data out-min out-min out-max out-max) (buffer->data buf)))))))

(deftest as-primitive-array-test
  (doseq [[buffer-data-type expected-class ^int byte-size expected-vec]
          [[:byte byte/1 Byte/BYTES (vector-of :byte Byte/MIN_VALUE 0 Byte/MAX_VALUE)]
           [:ubyte byte/1 Byte/BYTES (into (vector-of :byte) (map num/normalized->ubyte) [0.0 1.0])]
           [:short short/1 Short/BYTES (vector-of :short Short/MIN_VALUE 0 Short/MAX_VALUE)]
           [:ushort short/1 Short/BYTES (into (vector-of :short) (map num/normalized->ushort) [0.0 1.0])]
           [:int int/1 Integer/BYTES (vector-of :int Integer/MIN_VALUE 0 Integer/MAX_VALUE)]
           [:uint int/1 Integer/BYTES (into (vector-of :int) (map num/normalized->uint) [0.0 1.0])]
           [:long long/1 Long/BYTES (vector-of :long Long/MIN_VALUE 0 Long/MAX_VALUE)]
           [:float float/1 Float/BYTES (vector-of :float Float/MIN_VALUE 0.0 Float/MAX_VALUE)]
           [:double double/1 Double/BYTES (vector-of :double Double/MIN_VALUE 0.0 Double/MAX_VALUE)]]]
    (let [actual (-> (b/new-byte-buffer (* byte-size (count expected-vec)) :byte-order/native)
                     (b/put! 0 buffer-data-type false expected-vec)
                     (b/as-primitive-array buffer-data-type))]
      (is (= expected-class (class actual)))
      (is (= expected-vec (vec actual))))))
