(ns editor.buffers-test
  (:require [clojure.test :refer :all]
            [support.test-support :refer [array=]]
            [editor.buffers :as b])
  (:import [java.nio ByteBuffer]
           [com.google.protobuf ByteString]))

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
