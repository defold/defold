(ns editor.gl.vertex2-test
  (:require [clojure.test :refer :all]
            [support.test-support :refer [array=]]
            [editor.buffers :as b]
            [editor.gl.vertex2 :refer :all])
  (:import [java.nio ByteBuffer]
           [com.google.protobuf ByteString]
           [editor.gl.vertex2 VertexBuffer]))

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

(defvertex pos-1b
  (vec1.byte position))

(defvertex pos-2b
  (vec2.byte position))

(defvertex pos-2s
  (vec2.short position))

(deftest vertex-contains-correct-data
  (let [vertex-buffer (->pos-1b 1)]
    (pos-1b-put! vertex-buffer 42)

    (testing "what goes in comes out"
             (is (= 1    (count vertex-buffer)))
             (is (array= (byte-array [42])
                         (contents-of vertex-buffer))))

    (testing "once flipped, the data is still there"
             (let [final (flip! vertex-buffer)]
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
  (is (laid-out-as pos-1b  (range 10)
                   [0 1 2 3 4 5 6 7 8 9]))
  (is (laid-out-as pos-2b       (range 20)
                   [0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19]))
  (is (laid-out-as pos-2s (range 20)
                   [0 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10 0 11 0 12 0 13 0 14 0 15 0 16 0 17 0 18 0 19 0])))

(deftest attributes-compiled-correctly
  (is (= [{:components 1, :type :byte, :name "position", :normalized? false}] (:attributes pos-1b))))

(defvertex pos-4f-uv-2f
  (vec4.float position)
  (vec2.float texcoord))

(deftest two-vertex-contains-correct-data
  (let [vertex-buffer (->pos-4f-uv-2f 2)
        vertex-1      [100.0 101.0 102.0 103.0 150.0 151.0]
        vertex-2      [200.0 201.0 202.0 203.0 250.0 251.0]]
    (apply pos-4f-uv-2f-put! vertex-buffer vertex-1)
    (is (= 1 (count vertex-buffer)))
    (apply pos-4f-uv-2f-put! vertex-buffer vertex-2)
    (is (= 2 (count vertex-buffer)))))

(deftest equals
  (let [vertex-buffer-1 (->pos-4f-uv-2f 2)
        vertex-buffer-2 (->pos-4f-uv-2f 2)]
    (apply pos-4f-uv-2f-put! vertex-buffer-1 [100.0 101.0 102.0 103.0 150.0 151.0])
    (apply pos-4f-uv-2f-put! vertex-buffer-2 [100.0 101.0 102.0 103.0 150.0 151.0])
    (is (= (flip! vertex-buffer-1) (flip! vertex-buffer-2)))))
