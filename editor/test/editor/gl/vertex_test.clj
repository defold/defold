(ns editor.gl.vertex-test
  (:require [clojure.test :refer :all]
            [support.test-support :refer [array=]]
            [editor.buffers :as b]
            [editor.gl.vertex :refer :all])
  (:import [com.google.protobuf ByteString]))

(defn- contents-of
  [vb]
  (let [bb (.asReadOnlyBuffer (.buffer vb))
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

(defvertex one-d-position-only
  (vec1.byte location))

(defvertex two-d-position
  (vec2.byte position))

(defvertex two-d-position-short
  (vec2.short position))

(defvertex short-byte-byte
  (vec2.byte  bite)
  (vec2.short shorty)
  (vec1.byte  nibble))

(defvertex short-short :interleaved
  (vec1.short u)
  (vec1.short v))

(defvertex short-short-chunky :chunked
  (vec1.short u)
  (vec1.short v))

(deftest vertex-contains-correct-data
  (let [vertex-buffer (->one-d-position-only 1)]
    (conj! vertex-buffer [42])

    (testing "what goes in comes out"
             (is (= [42] (get vertex-buffer 0)))
             (is (= 1    (count vertex-buffer)))
             (is (array= (byte-array [42])
                         (contents-of vertex-buffer))))

    (testing "once made persistent, the data is still there"
             (let [final (persistent! vertex-buffer)]
               (is (= [42] (get final 0)))
               (is (= 1    (count final)))
               (is (array= (byte-array [42])
                           (contents-of final)))))))

(defn- laid-out-as [def into-seq expected-vec]
  (let [ctor  (symbol (str "->" (:name def)))
        dims  (reduce + (map second (:attributes def)))
        buf   ((ns-resolve 'editor.gl.vertex-test ctor) (/ (count into-seq) dims))]
    (doseq [x (partition dims into-seq)]
      (conj! buf x))

    (array= (byte-array expected-vec) (contents-of buf))))

(deftest memory-layouts
  (is (laid-out-as one-d-position-only  (range 10)
                   [0 1 2 3 4 5 6 7 8 9]))
  (is (laid-out-as two-d-position       (range 20)
                   [0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19]))
  (is (laid-out-as two-d-position-short (range 20)
                   [0 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10 0 11 0 12 0 13 0 14 0 15 0 16 0 17 0 18 0 19 0]))
  (is (laid-out-as short-byte-byte (interleave (range 0x10 0x18) (range 0x20 0x28) (range 0x100 0x108) (range 0x200 0x208) (map unchecked-byte (range 0x80 0x88)))
                   [;byte  byte   short       short       byte
                    0x10   0x20   0x00 0x01   0x00 0x02   0x80
                    0x11   0x21   0x01 0x01   0x01 0x02   0x81
                    0x12   0x22   0x02 0x01   0x02 0x02   0x82
                    0x13   0x23   0x03 0x01   0x03 0x02   0x83
                    0x14   0x24   0x04 0x01   0x04 0x02   0x84
                    0x15   0x25   0x05 0x01   0x05 0x02   0x85
                    0x16   0x26   0x06 0x01   0x06 0x02   0x86
                    0x17   0x27   0x07 0x01   0x07 0x02   0x87]))

  (is (laid-out-as short-short (interleave (range 0x188 0x190) (map unchecked-short (range 0x9000 0x9002)))
                   [;short        ;short
                    0x88 0x01     0x00 0x90   ;; u0 v0
                    0x89 0x01     0x01 0x90   ;; u1 v1
                    ]))

  (is (laid-out-as short-short-chunky (interleave (range 0x188 0x190) (map unchecked-short (range 0x9000 0x9002)))
                   [0x88 0x01 0x89 0x01   ;; u0 u1
                    0x00 0x90 0x01 0x90   ;; v0 v1
                    ])))

(deftest attributes-compiled-correctly
  (is (= [['location 1 'byte]] (:attributes one-d-position-only))))

(defvertex four-d-position-and-2d-texture
  (vec4.float position)
  (vec2.float texcoord))

(deftest four-two-vertex-contains-correct-data
  (let [vertex-buffer (->four-d-position-and-2d-texture 2)
        vertex-1      [100.0 101.0 102.0 103.0 150.0 151.0]
        vertex-2      [200.0 201.0 202.0 203.0 250.0 251.0]]
    (conj! vertex-buffer vertex-1)
    (is (= 1 (count vertex-buffer)))
    (conj! vertex-buffer vertex-2)
    (is (= 2 (count vertex-buffer)))
    (is (= vertex-1 (get vertex-buffer 0)))
    (is (= vertex-2 (get vertex-buffer 1)))))

(deftest equals
  (let [vertex-buffer-1 (persistent! (conj! (->four-d-position-and-2d-texture 2) [100.0 101.0 102.0 103.0 150.0 151.0]))
        vertex-buffer-2 (persistent! (conj! (->four-d-position-and-2d-texture 2) [100.0 101.0 102.0 103.0 150.0 151.0]))]
    (is (= vertex-buffer-1 vertex-buffer-2))))

(deftest byte-pack
  (testing "returns a ByteString containing contents before current position"
    (are [expected vertex-buffer] (array= (byte-array expected) (.toByteArray (b/byte-pack vertex-buffer)))
      [0 0 0 0] (reduce conj! (->two-d-position 2) [])
      [1 2 0 0] (reduce conj! (->two-d-position 2) [[1 2]])
      [1 2 3 4] (reduce conj! (->two-d-position 2) [[1 2] [3 4]])
      [0 0 0 0] (persistent! (reduce conj! (->two-d-position 2) []))
      [1 2 0 0] (persistent! (reduce conj! (->two-d-position 2) [[1 2]]))
      [1 2 3 4] (persistent! (reduce conj! (->two-d-position 2) [[1 2] [3 4]]))))
  (testing "multiple calls to byte-pack return the same value"
    (are [vertex-buffer] (let [vertex-buffer-val   vertex-buffer
                               byte-string1 (b/byte-pack vertex-buffer-val)
                               byte-string2 (b/byte-pack vertex-buffer-val)]
                           (array= (.toByteArray byte-string1) (.toByteArray byte-string2)))
      (reduce conj! (->two-d-position 2) [])
      (reduce conj! (->two-d-position 2) [[1 2]])
      (reduce conj! (->two-d-position 2) [[1 2] [3 4]])
      (persistent! (reduce conj! (->two-d-position 2) []))
      (persistent! (reduce conj! (->two-d-position 2) [[1 2]]))
      (persistent! (reduce conj! (->two-d-position 2) [[1 2] [3 4]])))))
