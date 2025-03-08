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

(ns editor.buffers
  (:require [util.num :as num])
  (:import [com.google.protobuf ByteString]
           [java.nio Buffer ByteBuffer ByteOrder DoubleBuffer FloatBuffer IntBuffer LongBuffer ShortBuffer]
           [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)

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

(defprotocol ByteStringCoding
  (byte-pack [source] "Return a Protocol Buffer compatible ByteString from the given source."))

(defn- new-buffer [s] (ByteBuffer/allocateDirect s))

(defn new-byte-buffer ^ByteBuffer [& dims] (new-buffer (reduce * 1 dims)))

(defn wrap-byte-array
  ^ByteBuffer [byte-array]
  (ByteBuffer/wrap byte-array))

(defn byte-order ^ByteBuffer [order ^ByteBuffer b] (.order b order))
(def ^ByteBuffer little-endian (partial byte-order ByteOrder/LITTLE_ENDIAN))
(def ^ByteBuffer big-endian    (partial byte-order ByteOrder/BIG_ENDIAN))
(def ^ByteBuffer native-order  (partial byte-order (ByteOrder/nativeOrder)))

(defn as-int-buffer ^IntBuffer [^ByteBuffer b] (.asIntBuffer b))

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

(defn primitive-type-kw [data-type]
  (case data-type
    (:double) :double
    (:float) :float
    (:long) :long
    (:int :uint) :int
    (:short :ushort) :short
    (:byte :ubyte) :byte))

(defn as-typed-buffer
  ^Buffer [^ByteBuffer buffer data-type]
  (case data-type
    (:double) (.asDoubleBuffer buffer)
    (:float) (.asFloatBuffer buffer)
    (:long) (.asLongBuffer buffer)
    (:int :uint) (.asIntBuffer buffer)
    (:short :ushort) (.asShortBuffer buffer)
    (:byte :ubyte) buffer))

(defn type-size
  ^long [type]
  (case type
    :ubyte Byte/BYTES
    :byte Byte/BYTES
    :ushort Short/BYTES
    :short Short/BYTES
    :uint Integer/BYTES
    :int Integer/BYTES
    :long Long/BYTES
    :float Float/BYTES
    :double Double/BYTES))

(defn item-byte-size
  ^long [buffer]
  (condp instance? buffer
    ByteBuffer Byte/BYTES
    DoubleBuffer Double/BYTES
    FloatBuffer Float/BYTES
    IntBuffer Integer/BYTES
    LongBuffer Long/BYTES
    ShortBuffer Short/BYTES))

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
  ^ByteBuffer [^ByteBuffer buffer byte-offset data-type normalize numbers]
  (let [byte-offset (int byte-offset)]
    (if normalize
      ;; Normalized.
      (case data-type
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
                numbers))

      ;; Not normalized.
      (case data-type
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
                numbers))))
  buffer)

(defn push-floats!
  ^ByteBuffer [^ByteBuffer buffer numbers]
  (doseq [n numbers]
    (.putFloat buffer (float n)))
  buffer)

(defn push!
  ^ByteBuffer [^ByteBuffer buffer data-type normalize numbers]
  (if normalize
    ;; Normalized.
    (case data-type
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
        (.putInt buffer (num/normalized->uint n))))

    ;; Not normalized.
    (case data-type
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
        (.putInt buffer (num/uint n)))))
  buffer)
