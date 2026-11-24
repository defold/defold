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

(ns integration.buffer-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [util.murmur :as murmur])
  (:import [java.nio ByteBuffer ByteOrder]))

(set! *warn-on-reflection* true)

(defn- expected-buffer
  ^bytes [value]
  (let [bytes (doto (ByteBuffer/allocate 56)
                (.order ByteOrder/LITTLE_ENDIAN)
                (.put (byte-array [66 85 70 67 1 0 2 0 1 0 0 0 8 0 0 0]))
                (.putLong (murmur/hash64 "color"))
                (.putInt 0)
                (.put (byte 0))
                (.put (byte 3))
                (.putShort (short 0))
                (.putLong (murmur/hash64 "weight"))
                (.putInt 4)
                (.put (byte 8))
                (.put (byte 1))
                (.putShort (short 0))
                (.put (byte-array [1 2 3 0]))
                (.putFloat (float value)))]
    (.array bytes)))

;; Builds pack in-memory stream data and invalidate output after unsaved edits.
(deftest build-unsaved-buffer
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [node-id (test-util/make-code-resource-node!
                    project "/packed.buffer"
                    "[{\"name\":\"color\",\"type\":\"uint8\",\"count\":3,\"data\":[1,2,3]},
                      {\"name\":\"weight\",\"type\":\"float32\",\"count\":1,\"data\":[-1.5]}]")]
      (is (g/node-value node-id :dirty))
      (test-util/build-node! node-id nil)
      (is (= (vec (expected-buffer -1.5))
             (vec (test-util/node-build-output node-id))))
      (test-util/set-code-editor-source!
        node-id
        "[{\"name\":\"color\",\"type\":\"uint8\",\"count\":3,\"data\":[1,2,3]},
          {\"name\":\"weight\",\"type\":\"float32\",\"count\":1,\"data\":[7]}]")
      (test-util/build-node! node-id nil)
      (is (= (vec (expected-buffer 7))
             (vec (test-util/node-build-output node-id)))))))

;; Invalid layouts surface as build errors instead of unreadable resources.
(deftest invalid-buffer-layout
  (test-util/with-scratch-project "test/resources/empty_project"
    (let [node-id (test-util/make-code-resource-node!
                    project "/invalid.buffer"
                    "[{\"name\":\"position\",\"type\":\"float32\",\"count\":0}]")]
      (is (g/error? (test-util/build-error! node-id))))))
