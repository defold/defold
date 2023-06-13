;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.gl.vertex2-test
  (:require [clojure.test :refer :all]
            [editor.gl.vertex2 :as v]
            [support.test-support :refer [array=]])
  (:import [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteBuffer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- contents-of ^bytes
  [^VertexBuffer vb]
  (let [bb (.asReadOnlyBuffer ^ByteBuffer (.buf vb))
        arr (byte-array (.limit bb))]
    (.rewind bb)
    (.get bb arr)
    arr))

(defn- print-buffer
  [b cols]
  (let [fmt  (clojure.string/join " " (repeat cols "%02X "))
        rows (partition cols (seq (contents-of b)))]
    (doseq [r rows]
      (println (apply format fmt r)))))

(v/defvertex pos-1b
  (vec1.byte position))

(v/defvertex pos-2b
  (vec2.byte position))

(v/defvertex pos-2s
  (vec2.short position))

(deftest vertex-contains-correct-data
  (let [vertex-buffer (->pos-1b 1)]
    (pos-1b-put! vertex-buffer 42)

    (testing "what goes in comes out"
             (is (= 1    (count vertex-buffer)))
             (is (array= (byte-array [42])
                         (contents-of vertex-buffer))))

    (testing "once flipped, the data is still there"
             (let [final (v/flip! vertex-buffer)]
               (is (= 1    (count final)))
               (is (array= (byte-array [42])
                           (contents-of final)))))))

(defn- laid-out-as [def into-seq expected-vec]
  (let [ctor  (symbol (str "->" (:name def)))
        dims  (reduce + (map :components (:attributes def)))
        buf ((ns-resolve 'editor.gl.vertex2-test ctor) (/ (count into-seq) ^long dims))
        put! (ns-resolve 'editor.gl.vertex2-test (symbol (str (:name def) "-put!")))]
    (doseq [x (partition dims into-seq)]
      (apply put! buf x))
    (array= (byte-array expected-vec) (contents-of buf))))

(deftest vertex-constructor
  (is (thrown? AssertionError (->pos-1b 1 :invalid-usage))))

(deftest memory-layouts
  (is (laid-out-as pos-1b (range 10)
                   [0 1 2 3 4 5 6 7 8 9]))
  (is (laid-out-as pos-2b (range 20)
                   [0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19]))
  (is (laid-out-as pos-2s (range 20)
                   [0 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10 0 11 0 12 0 13 0 14 0 15 0 16 0 17 0 18 0 19 0])))

(deftest attributes-compiled-correctly
  (is (= [{:name "position"
           :name-key :position
           :type :byte
           :components 1
           :normalize false
           :semantic-type :semantic-type-position
           :coordinate-space :coordinate-space-world}]
         (:attributes pos-1b))))

(v/defvertex pos-4f-uv-2f
  (vec4.float position)
  (vec2.float texcoord))

(deftest two-vertex-contains-correct-data
  (let [vertex-buffer (->pos-4f-uv-2f 2)
        vertex-1 [100.0 101.0 102.0 103.0 150.0 151.0]
        vertex-2 [200.0 201.0 202.0 203.0 250.0 251.0]]
    (apply pos-4f-uv-2f-put! vertex-buffer vertex-1)
    (is (= 1 (count vertex-buffer)))
    (apply pos-4f-uv-2f-put! vertex-buffer vertex-2)
    (is (= 2 (count vertex-buffer)))))

;; -----------------------------------------------------------------------------
;; Buffer tests
;; -----------------------------------------------------------------------------

(defn- primitive-type-kw [data-type]
  (case data-type
    (:double) :double
    (:float) :float
    (:int :uint) :int
    (:short :ushort) :short
    (:byte :ubyte) :byte))

(defn- element-byte-size
  ^long [data-type]
  (case data-type
    (:double) Double/BYTES
    (:float) Float/BYTES
    (:int :uint) Integer/BYTES
    (:short :ushort) Short/BYTES
    (:byte :ubyte) Byte/BYTES))

(defn- element-at-offset-fn [data-type]
  (case data-type
    (:double) #(.getDouble ^ByteBuffer %1 ^long %2)
    (:float) #(.getFloat ^ByteBuffer %1 ^long %2)
    (:int :uint) #(.getInt ^ByteBuffer %1 ^long %2)
    (:short :ushort) #(.getShort ^ByteBuffer %1 ^long %2)
    (:byte :ubyte) #(.get ^ByteBuffer %1 ^long %2)))

(defn- buf->data-fn [data-type]
  (let [primitive-type-kw (primitive-type-kw data-type)
        element-byte-size (element-byte-size data-type)
        element-at-offset-fn (element-at-offset-fn data-type)]
    (fn buf->data [^ByteBuffer buf]
      (into (vector-of primitive-type-kw)
            (map #(element-at-offset-fn buf %))
            (range 0 (.capacity buf) element-byte-size)))))

(defn- buf->bytes [^ByteBuffer buf]
  (into (vector-of :byte)
        (map #(.get buf ^long %))
        (range 0 (.capacity buf) Byte/BYTES)))

(defn- data-fn [data-type]
  (partial vector-of (primitive-type-kw data-type)))

(deftest buf-blit!-test
  (let [min Byte/MIN_VALUE
        max Byte/MAX_VALUE
        buf->data (buf->data-fn :byte)
        data (data-fn :byte)
        data-bytes (comp byte-array data)
        buf (v/make-buf 4)]
    (is (identical? buf (v/buf-blit! buf 0 (data-bytes min min min min))) "Returns self")
    (is (zero? (.position buf)) "Position is unaffected")
    (is (= (data min min min min) (buf->data buf)))
    (v/buf-blit! buf 1 (data-bytes max max))
    (is (= (data min max max min) (buf->data buf)))))

(deftest buf-put-floats!-test
  (let [min Float/MIN_VALUE
        max Float/MAX_VALUE
        element-byte-size (element-byte-size :float)
        buf->data (buf->data-fn :float)
        data (data-fn :float)
        buf (v/make-buf (* 4 element-byte-size))]
    (is (identical? buf (v/buf-put-floats! buf 0 (data min min min min))) "Put returns self")
    (is (zero? (.position buf)) "Position is unaffected")
    (is (= (data min min min min) (buf->data buf)))
    (v/buf-put-floats! buf element-byte-size (data max max))
    (is (= (data min max max min) (buf->data buf)))))

(deftest buf-put!-test
  (doseq [normalize [false true]
          data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
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
          (case data-type
            (:double :float) out-max
            (:int :uint :short :ushort :byte :ubyte) (if normalize 1.0 out-max))

          element-byte-size (element-byte-size data-type)
          buf->data (buf->data-fn data-type)
          data (data-fn data-type)
          buf (v/make-buf (* 4 element-byte-size))]

      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name data-type))
        (is (identical? buf (v/buf-put! buf 0 data-type normalize (data in-min in-min in-min in-min))) "Returns self")
        (is (zero? (.position buf)) "Position is unaffected")
        (is (= (data out-min out-min out-min out-min) (buf->data buf)))
        (v/buf-put! buf element-byte-size data-type normalize (data in-max in-max))
        (is (= (data out-min out-max out-max out-min) (buf->data buf)))))))

(deftest buf-push-floats!-test
  (let [min Float/MIN_VALUE
        max Float/MAX_VALUE
        element-byte-size (element-byte-size :float)
        buf->data (buf->data-fn :float)
        data (data-fn :float)
        buf (v/make-buf (* 4 element-byte-size))]
    (is (identical? buf (v/buf-push-floats! buf (data min min))) "Returns self")
    (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
    (is (= (data min min 0 0) (buf->data buf)))
    (v/buf-push-floats! buf (data max max))
    (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
    (is (= (data min min max max) (buf->data buf)))))

(deftest buf-push!-test
  (doseq [normalize [false true]
          data-type [:double :float :int :uint :short :ushort :byte :ubyte]]
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
          (case data-type
            (:double :float) out-max
            (:int :uint :short :ushort :byte :ubyte) (if normalize 1.0 out-max))

          element-byte-size (element-byte-size data-type)
          buf->data (buf->data-fn data-type)
          data (data-fn data-type)
          buf (v/make-buf (* 4 element-byte-size))]

      (testing (format "%s %ss" (if normalize "Normalized" "Non-normalized") (name data-type))
        (is (identical? buf (v/buf-push! buf data-type normalize (data in-min in-min))) "Returns self")
        (is (= (* 2 element-byte-size) (.position buf)) "Position advances")
        (is (= (data out-min out-min 0 0) (buf->data buf)))
        (v/buf-push! buf data-type normalize (data in-max in-max))
        (is (= (* 4 element-byte-size) (.position buf)) "Position advances")
        (is (= (data out-min out-min out-max out-max) (buf->data buf)))))))
