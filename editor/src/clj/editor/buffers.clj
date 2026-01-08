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

(ns editor.buffers
  (:refer-clojure :exclude [empty?])
  (:require [internal.java :as java]
            [util.array :as array]
            [util.defonce :as defonce]
            [util.fn :as fn]
            [util.num :as num])
  (:import [clojure.lang Counted IHashEq IReduceInit Murmur3]
           [com.google.protobuf ByteString]
           [java.nio Buffer ByteBuffer ByteOrder CharBuffer DoubleBuffer FloatBuffer IntBuffer LongBuffer ShortBuffer]
           [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn slurp-bytes
  ^bytes [^ByteBuffer buff]
  (let [buff (.duplicate buff)
        n (.remaining buff)
        bytes (byte-array n)]
    (.get buff bytes)
    bytes))

(defn alias-buf-bytes
  "Avoids copy if possible."
  ^bytes [^ByteBuffer buff]
  (if (and (.hasArray buff) (= (.remaining buff) (alength (.array buff))))
    (.array buff)
    (slurp-bytes buff)))

(defn bbuf->string
  (^String [^ByteBuffer bb]
   (String. (alias-buf-bytes bb) StandardCharsets/UTF_8))
  (^String [^ByteBuffer bb ^long offset ^long length]
   (String. (alias-buf-bytes bb) offset length StandardCharsets/UTF_8)))

(defonce/protocol ByteStringCoding
  (byte-pack [source] "Return a Protocol Buffer compatible ByteString from the given source."))

(defonce ^:private native-java-nio-byte-order (ByteOrder/nativeOrder))

(defn- byte-order->java-nio-byte-order
  ^ByteOrder [byte-order]
  (case byte-order
    :byte-order/native native-java-nio-byte-order
    :byte-order/big-endian ByteOrder/BIG_ENDIAN
    :byte-order/little-endian ByteOrder/LITTLE_ENDIAN))

(defn new-byte-buffer
  ^ByteBuffer [byte-size byte-order]
  (let [java-nio-byte-order (byte-order->java-nio-byte-order byte-order)]
    (doto (ByteBuffer/allocateDirect byte-size)
      (.order java-nio-byte-order))))

(defn wrap-byte-array
  ^ByteBuffer [^bytes byte-array byte-order]
  (let [java-nio-byte-order (byte-order->java-nio-byte-order byte-order)]
    (doto (ByteBuffer/wrap byte-array)
      (.order java-nio-byte-order))))

(defn new-float-buffer
  ^FloatBuffer [capacity byte-order]
  (let [byte-size (* Float/BYTES (int capacity))
        byte-buffer (new-byte-buffer byte-size byte-order)]
    (.asFloatBuffer byte-buffer)))

(defn wrap-float-array
  ^FloatBuffer [^floats float-array]
  (FloatBuffer/wrap float-array))

(defonce/protocol HasByteOrder
  "Annoyingly, there is no method on the Buffer interface to retrieve the byte
  order. However, they all have one."
  (byte-order ^ByteOrder [buffer] "Returns the java.nio.ByteOrder of the Buffer."))

(extend-protocol HasByteOrder
  ByteBuffer
  (byte-order [buffer] (.order buffer))

  CharBuffer
  (byte-order [buffer] (.order buffer))

  DoubleBuffer
  (byte-order [buffer] (.order buffer))

  FloatBuffer
  (byte-order [buffer] (.order buffer))

  IntBuffer
  (byte-order [buffer] (.order buffer))

  LongBuffer
  (byte-order [buffer] (.order buffer))

  ShortBuffer
  (byte-order [buffer] (.order buffer)))

(defonce/protocol ReducerImpl
  (read-only [buffer] "Returns a new read-only Buffer that shares the content of this Buffer. The position, limit, and mark states are independent of the original Buffer.")
  (next-element [buffer] "Returns the element at the current position of the Buffer, then advances the position of the Buffer."))

(extend-protocol ReducerImpl
  ByteBuffer
  (read-only [buffer] (.order (.asReadOnlyBuffer buffer) (.order buffer)))
  (next-element [buffer] (.get buffer))

  CharBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer))

  DoubleBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer))

  FloatBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer))

  IntBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer))

  LongBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer))

  ShortBuffer
  (read-only [buffer] (.asReadOnlyBuffer buffer))
  (next-element [buffer] (.get buffer)))

(defn reducible
  ^IReduceInit [^Buffer buffer]
  (when buffer
    (let [^Buffer view (read-only buffer)]
      (reify IReduceInit
        (reduce [_ rf init]
          (.rewind view)
          (loop [result init]
            (if (.hasRemaining view)
              (let [element (next-element view)
                    result (rf result element)]
                (if (reduced? result)
                  (unreduced result)
                  (recur result)))
              result)))))))

(defn copy-buffer ^ByteBuffer [^ByteBuffer b]
  (let [sz      (.capacity b)
        clone   (if (.isDirect b) (ByteBuffer/allocateDirect sz) (ByteBuffer/allocate sz))
        ro-copy (.asReadOnlyBuffer b)]
    (.flip ro-copy)
    (.put clone ro-copy)))

(defn slice [^ByteBuffer bb offsets]
  (let [dup (.duplicate bb)]
    (mapv (fn [o] (do
                   (.position dup (int o))
                   (doto (.slice dup)
                     (.order (.order bb))))) offsets)))

(extend-type ByteBuffer
  ByteStringCoding
  (byte-pack [this] (ByteString/copyFrom (.asReadOnlyBuffer this))))

(defn make-put-fn-form [^long component-count source-data-tag-sym put-method-sym]
  (let [offset-sym (gensym "offset")
        source-data-sym (gensym "source-data")
        byte-buffer-sym (gensym "byte-buffer")
        vertex-index-sym (gensym "vertex-index")]
    `(fn [~(with-meta source-data-sym {:tag source-data-tag-sym})
          ~(with-meta byte-buffer-sym {:tag `ByteBuffer})
          ~(with-meta vertex-index-sym {:tag `long})]
       (let [~offset-sym (* ~vertex-index-sym ~component-count)]
         ~@(map (fn [^long component-index]
                  `(. ~byte-buffer-sym ~put-method-sym (aget ~source-data-sym (+ ~offset-sym ~component-index))))
                (range component-count))))))

(defonce buffer-data-types
  #{:ubyte
    :byte
    :ushort
    :short
    :uint
    :int
    :long
    :float
    :double})

(fn/defamong buffer-data-type? buffer-data-types)

(defn primitive-type-kw [buffer-data-type]
  (case buffer-data-type
    (:double) :double
    (:float) :float
    (:long) :long
    (:int :uint) :int
    (:short :ushort) :short
    (:byte :ubyte) :byte))

(defn as-typed-buffer
  ^Buffer [^ByteBuffer buffer buffer-data-type]
  (case buffer-data-type
    (:double) (.asDoubleBuffer buffer)
    (:float) (.asFloatBuffer buffer)
    (:long) (.asLongBuffer buffer)
    (:int :uint) (.asIntBuffer buffer)
    (:short :ushort) (.asShortBuffer buffer)
    (:byte :ubyte) buffer))

(defn type-size
  ^long [buffer-data-type]
  (case buffer-data-type
    :ubyte Byte/BYTES
    :byte Byte/BYTES
    :ushort Short/BYTES
    :short Short/BYTES
    :uint Integer/BYTES
    :int Integer/BYTES
    :long Long/BYTES
    :float Float/BYTES
    :double Double/BYTES))

(definline item-count [^Buffer buffer]
  `(int
     (if-let [^Buffer buffer# ~buffer]
       (- (.limit buffer#)
          (.position buffer#))
       0)))

(definline empty? [^Buffer buffer]
  `(zero? (item-count ~buffer)))

(definline flipped? [^Buffer buffer]
  `(zero? (.position ~(with-meta buffer {:tag `Buffer}))))

(defn item-byte-size
  ^long [buffer]
  (condp instance? buffer
    ByteBuffer Byte/BYTES
    CharBuffer Character/BYTES
    DoubleBuffer Double/BYTES
    FloatBuffer Float/BYTES
    IntBuffer Integer/BYTES
    LongBuffer Long/BYTES
    ShortBuffer Short/BYTES))

(defn total-byte-size
  ^long [^Buffer buffer]
  (* (item-byte-size buffer)
     (item-count buffer)))

(defn as-primitive-array
  [^ByteBuffer buffer buffer-data-type]
  (case buffer-data-type
    (:double)
    (let [size (quot (item-count buffer) Double/BYTES)
          array (double-array size)]
      (.get (.asDoubleBuffer buffer) array)
      array)

    (:float)
    (let [size (quot (item-count buffer) Float/BYTES)
          array (float-array size)]
      (.get (.asFloatBuffer buffer) array)
      array)

    (:long)
    (let [size (quot (item-count buffer) Long/BYTES)
          array (long-array size)]
      (.get (.asLongBuffer buffer) array)
      array)

    (:int :uint)
    (let [size (quot (item-count buffer) Integer/BYTES)
          array (int-array size)]
      (.get (.asIntBuffer buffer) array)
      array)

    (:short :ushort)
    (let [size (quot (item-count buffer) Short/BYTES)
          array (short-array size)]
      (.get (.asShortBuffer buffer) array)
      array)

    (:byte :ubyte)
    (let [size (item-count buffer)
          array (byte-array size)]
      (.get buffer array)
      array)))

(defn data-hash
  ^long [^Buffer buffer]
  (if (.hasArray buffer)
    (let [start (.arrayOffset buffer)
          end (+ start (.limit buffer))]
      (array/hash-code (.array buffer) start end))
    (let [original-position (.position buffer)]
      (.rewind buffer)
      (try
        (.hashCode buffer)
        (finally
          (.position buffer original-position))))))

(defn topology-hash
  ^long [^Buffer buffer]
  (java/combine-hashes
    (hash (class buffer))
    (.hashCode (byte-order buffer))
    (Murmur3/hashLong (total-byte-size buffer))))

(defn topology-equals? [^Buffer a ^Buffer b]
  (and (= (class a) (class b))
       (= (byte-order a) (byte-order b))
       (= (total-byte-size a) (total-byte-size b))))

(defn- byte-order-rank
  ^long [^ByteOrder byte-order]
  ;; We use rank 0 for BIG_ENDIAN since the order of a newly-created byte buffer
  ;; is always BIG_ENDIAN. Thus, LITTLE_ENDIAN deviates from the default and
  ;; should have a non-zero value.
  (condp = byte-order
    ByteOrder/BIG_ENDIAN 0
    ByteOrder/LITTLE_ENDIAN 1))

(defn- byte-order-comparison
  ^long [^ByteOrder a ^ByteOrder b]
  (Long/compare (byte-order-rank a)
                (byte-order-rank b)))

(defn topology-comparison
  ^long [^Buffer a ^Buffer b]
  (if (or (nil? a) (nil? b))
    (compare a b)
    (if (not= (class a) (class b))
      (throw (ClassCastException. (str (.getName (class a)) " cannot be compared to " (.getName (class b)))))
      (let [byte-order-comparison (byte-order-comparison (byte-order a) (byte-order b))]
        (if-not (zero? byte-order-comparison)
          byte-order-comparison
          (compare (total-byte-size a) (total-byte-size b)))))))

;; A wrapper around Buffer with equality semantics based on the topology of the
;; Buffer and an explicit data version number.
(defonce/type BufferData
  [^Buffer data
   ^int data-version
   ^int topology-hash]

  Comparable
  (compareTo [_this that]
    (let [^BufferData that that]
      (java/combine-comparisons
        (Integer/compare data-version (.-data-version that))
        (Integer/compare topology-hash (.-topology-hash that)))))

  Counted
  (count [_this]
    (item-count data))

  IHashEq
  (hasheq [_this]
    (java/combine-hashes topology-hash data-version))

  IReduceInit
  (reduce [_this rf init]
    (let [^Buffer view (read-only data)]
      (loop [result init]
        (if (.hasRemaining view)
          (let [element (next-element view)
                result (rf result element)]
            (if (reduced? result)
              (unreduced result)
              (recur result)))
          result))))

  Object
  (hashCode [this]
    (.hasheq this))

  (equals [this that]
    (or (identical? this that)
        (let [^BufferData that that]
          (and (instance? BufferData that)
               (= data-version (.-data-version that))
               (= topology-hash (.-topology-hash that))))))

  (toString [_this]
    (format "data=[pos=%d lim=%d cap=%d] ver=%#x"
            (.position data)
            (.limit data)
            (.capacity data)
            data-version)))

(defn make-buffer-data
  (^BufferData [^Buffer data]
   (let [data-version (data-hash data)]
     (make-buffer-data data data-version)))
  (^BufferData [^Buffer data ^long data-version]
   (cond
     (nil? data)
     (throw (IllegalArgumentException. "data Buffer cannot be nil"))

     (not (flipped? data))
     (throw (IllegalArgumentException. "data Buffer must be flipped"))

     :else
     (let [topology-hash (topology-hash data)]
       (->BufferData data data-version topology-hash)))))

(defn invalidate-buffer-data
  ^BufferData [^BufferData buffer-data]
  (->BufferData
    (.-data buffer-data)
    (unchecked-inc-int (.-data-version buffer-data))
    (.-topology-hash buffer-data)))

(defn blit!
  ^ByteBuffer [^ByteBuffer buffer ^long byte-offset ^bytes bytes]
  (let [old-position (.position buffer)]
    (.position buffer byte-offset)
    (.put buffer bytes)
    (.position buffer old-position)))

(defn put-floats!
  ^ByteBuffer [^ByteBuffer buffer ^long byte-offset numbers]
  (reduce (fn [^long byte-offset n]
            (.putFloat buffer byte-offset (float n))
            (+ byte-offset Float/BYTES))
          byte-offset
          numbers)
  buffer)

(defn put!
  ^ByteBuffer [^ByteBuffer buffer byte-offset buffer-data-type normalize numbers]
  (let [byte-offset (int byte-offset)]
    (if normalize
      ;; Normalized.
      (case buffer-data-type
        :float
        (reduce (fn [^long byte-offset n]
                  (.putFloat buffer byte-offset (float n))
                  (+ byte-offset Float/BYTES))
                byte-offset
                numbers)

        :double
        (reduce (fn [^long byte-offset n]
                  (.putDouble buffer byte-offset (double n))
                  (+ byte-offset Double/BYTES))
                byte-offset
                numbers)

        :byte
        (reduce (fn [^long byte-offset n]
                  (.put buffer byte-offset (num/normalized->byte n))
                  (inc byte-offset))
                byte-offset
                numbers)

        :ubyte
        (reduce (fn [^long byte-offset n]
                  (.put buffer byte-offset (num/normalized->ubyte n))
                  (inc byte-offset))
                byte-offset
                numbers)

        :short
        (reduce (fn [^long byte-offset n]
                  (.putShort buffer byte-offset (num/normalized->short n))
                  (+ byte-offset Short/BYTES))
                byte-offset
                numbers)

        :ushort
        (reduce (fn [^long byte-offset n]
                  (.putShort buffer byte-offset (num/normalized->ushort n))
                  (+ byte-offset Short/BYTES))
                byte-offset
                numbers)

        :int
        (reduce (fn [^long byte-offset n]
                  (.putInt buffer byte-offset (num/normalized->int n))
                  (+ byte-offset Integer/BYTES))
                byte-offset
                numbers)

        :uint
        (reduce (fn [^long byte-offset n]
                  (.putInt buffer byte-offset (num/normalized->uint n))
                  (+ byte-offset Integer/BYTES))
                byte-offset
                numbers)

        :long
        (reduce (fn [^long byte-offset n]
                  (.putLong buffer byte-offset (num/normalized->long n))
                  (+ byte-offset Long/BYTES))
                byte-offset
                numbers))

      ;; Not normalized.
      (case buffer-data-type
        :float
        (reduce (fn [^long byte-offset n]
                  (.putFloat buffer byte-offset (float n))
                  (+ byte-offset Float/BYTES))
                byte-offset
                numbers)

        :double
        (reduce (fn [^long byte-offset n]
                  (.putDouble buffer byte-offset (double n))
                  (+ byte-offset Double/BYTES))
                byte-offset
                numbers)

        :byte
        (reduce (fn [^long byte-offset n]
                  (.put buffer byte-offset (byte n))
                  (inc byte-offset))
                byte-offset
                numbers)

        :ubyte
        (reduce (fn [^long byte-offset n]
                  (.put buffer byte-offset (num/ubyte n))
                  (inc byte-offset))
                byte-offset
                numbers)

        :short
        (reduce (fn [^long byte-offset n]
                  (.putShort buffer byte-offset (short n))
                  (+ byte-offset Short/BYTES))
                byte-offset
                numbers)

        :ushort
        (reduce (fn [^long byte-offset n]
                  (.putShort buffer byte-offset (num/ushort n))
                  (+ byte-offset Short/BYTES))
                byte-offset
                numbers)

        :int
        (reduce (fn [^long byte-offset n]
                  (.putInt buffer byte-offset (int n))
                  (+ byte-offset Integer/BYTES))
                byte-offset
                numbers)

        :uint
        (reduce (fn [^long byte-offset n]
                  (.putInt buffer byte-offset (num/uint n))
                  (+ byte-offset Integer/BYTES))
                byte-offset
                numbers)

        :long
        (reduce (fn [^long byte-offset n]
                  (.putLong buffer byte-offset (long n))
                  (+ byte-offset Long/BYTES))
                byte-offset
                numbers))))
  buffer)

(defn push-floats!
  ^ByteBuffer [^ByteBuffer buffer numbers]
  (run! (fn [n]
          (.putFloat buffer (float n)))
        numbers)
  buffer)

(defn push!
  ^ByteBuffer [^ByteBuffer buffer buffer-data-type normalize numbers]
  (if normalize
    ;; Normalized.
    (case buffer-data-type
      :float
      (doseq [n numbers]
        (.putFloat buffer (float n)))

      :double
      (doseq [n numbers]
        (.putDouble buffer (double n)))

      :byte
      (doseq [^double n numbers]
        (.put buffer ^byte (num/normalized->byte n)))

      :ubyte
      (doseq [^double n numbers]
        (.put buffer ^byte (num/normalized->ubyte n)))

      :short
      (doseq [^double n numbers]
        (.putShort buffer (num/normalized->short n)))

      :ushort
      (doseq [^double n numbers]
        (.putShort buffer (num/normalized->ushort n)))

      :int
      (doseq [^double n numbers]
        (.putInt buffer (num/normalized->int n)))

      :uint
      (doseq [^double n numbers]
        (.putInt buffer (num/normalized->uint n)))

      :long
      (doseq [^double n numbers]
        (.putLong buffer (num/normalized->long n))))

    ;; Not normalized.
    (case buffer-data-type
      :float
      (doseq [n numbers]
        (.putFloat buffer (float n)))

      :double
      (doseq [n numbers]
        (.putDouble buffer (double n)))

      :byte
      (doseq [n numbers]
        (.put buffer (byte n)))

      :ubyte
      (doseq [n numbers]
        (.put buffer ^byte (num/ubyte n)))

      :short
      (doseq [n numbers]
        (.putShort buffer (short n)))

      :ushort
      (doseq [n numbers]
        (.putShort buffer (num/ushort n)))

      :int
      (doseq [n numbers]
        (.putInt buffer (int n)))

      :uint
      (doseq [n numbers]
        (.putInt buffer (num/uint n)))

      :long
      (doseq [n numbers]
        (.putLong buffer (long n)))))
  buffer)
