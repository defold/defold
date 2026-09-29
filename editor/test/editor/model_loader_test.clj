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

(ns editor.model-loader-test
  (:require [clojure.test :refer :all]
            [editor.model-loader :as model-loader]
            [util.coll :as coll]
            [util.digest :as digest])
  (:import [java.io ByteArrayInputStream]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.security DigestInputStream]))

(set! *warn-on-reflection* true)

(defn- glb-bytes [json-string binary-byte-count]
  (let [^bytes json-bytes (.getBytes ^String json-string StandardCharsets/UTF_8)
        json-byte-count (alength json-bytes)
        padded-json-byte-count (bit-and (+ json-byte-count 3) -4)
        total-byte-count (+ 20 padded-json-byte-count 8 binary-byte-count)
        ^ByteBuffer byte-buffer (doto (ByteBuffer/allocate total-byte-count)
                                  (.order ByteOrder/LITTLE_ENDIAN))]
    (.putInt byte-buffer 0x46546c67)
    (.putInt byte-buffer 2)
    (.putInt byte-buffer total-byte-count)
    (.putInt byte-buffer padded-json-byte-count)
    (.putInt byte-buffer 0x4e4f534a)
    (.put byte-buffer json-bytes)
    (dotimes [_ (- padded-json-byte-count json-byte-count)]
      (.put byte-buffer (byte 0x20)))
    (.putInt byte-buffer binary-byte-count)
    (.putInt byte-buffer 0x004e4942)
    (dotimes [_ binary-byte-count]
      (.put byte-buffer (byte 0x7f)))
    (.array byte-buffer)))

(deftest named-mesh-choicebox-options-test
  (let [options (model-loader/named-mesh-choicebox-options
                  [{:index 0 :name "Tree" :name-generated false}
                   {:index 1 :name "Tree" :name-generated false}])]
    (is (= [[-1 ""] [0 "Tree (raw0)"] [1 "Tree (raw1)"]]
           options))
    (is (coll/every? (comp string? second) options))))

(deftest read-external-buffer-uris-gltf-test
  (let [json-string (str "{\"buffers\":["
                         "{\"uri\":\"first.bin\"},"
                         "{\"uri\":\"data:application/octet-stream;base64,AA==\"},"
                         "{\"uri\":\"\"},"
                         "{\"uri\":\"first.bin\"},"
                         "{\"uri\":\"second.bin\"}]}")
        ^ByteArrayInputStream input-stream (ByteArrayInputStream. (.getBytes json-string StandardCharsets/UTF_8))]
    (is (= ["first.bin" "second.bin"]
           (model-loader/read-external-buffer-uris input-stream)))
    (is (zero? (.available input-stream)))))

(deftest read-external-buffer-uris-glb-test
  (let [binary-byte-count 4096
        json-string "{\"buffers\":[{\"uri\":\"external.bin\"}]}"
        ^bytes bytes (glb-bytes json-string binary-byte-count)
        expected-sha256 (with-open [input-stream (ByteArrayInputStream. bytes)]
                          (digest/stream->sha256-hex input-stream))]
    (with-open [^DigestInputStream input-stream (digest/make-digest-input-stream (ByteArrayInputStream. bytes) "SHA-256")]
      (is (= ["external.bin"]
             (model-loader/read-external-buffer-uris input-stream)))
      (is (zero? (.available input-stream)))
      (is (= expected-sha256
             (digest/digest-input-stream->hex input-stream))))))

(deftest read-external-buffer-uris-malformed-input-test
  (doseq [^bytes bytes [(.getBytes "{" StandardCharsets/UTF_8)
                        (byte-array [(int \g) (int \l) (int \T) (int \F)])]]
    (let [expected-sha256 (with-open [input-stream (ByteArrayInputStream. bytes)]
                            (digest/stream->sha256-hex input-stream))]
      (with-open [^DigestInputStream input-stream (digest/make-digest-input-stream (ByteArrayInputStream. bytes) "SHA-256")]
        (is (= []
               (model-loader/read-external-buffer-uris input-stream)))
        (is (zero? (.available input-stream)))
        (is (= expected-sha256
               (digest/digest-input-stream->hex input-stream)))))))
