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

(ns util.digestable-test
  (:require [clojure.test :refer :all]
            [util.digestable :as digestable])
  (:import [com.defold.util IDigestable]
           [java.io StringWriter Writer]))

(set! *warn-on-reflection* true)

(defn- write-props [^Writer writer ^String id]
  (.write writer "{:id ")
  (.write writer (if id id "nil"))
  (.write writer "}"))

(deftype DigestableType [^String id]
  IDigestable
  (digest [_this writer]
    (write-props writer id)))

(deftype OtherDigestableType [^String id]
  IDigestable
  (digest [_this writer]
    (write-props writer id)))

(defn- digest-written
  ^String [^IDigestable digestable]
  (let [writer (StringWriter.)]
    (.digest digestable writer)
    (.toString writer)))

(deftest sha1-hash-test
  (testing "Excludes digest-ignored prefixed fields from maps."
    (is (= (digestable/sha1-hash {})
           (digestable/sha1-hash {:digest-ignored/key "value"}))))

  (testing "Digests classes that implement the IDigestable interface."

    (testing "Written data is included in the hash."
      (is (= "922a703f6356f3b4b16bfe6089fc7891de4da2c0"
             (digestable/sha1-hash (->DigestableType "one"))))
      (is (= "028f9c27f1e8f3ec319124cfa2de0f94cc41b9a7"
             (digestable/sha1-hash (->DigestableType "two")))))

    (testing "Type information is included in the hash."
      (is (= "{:id nil}" (digest-written (->DigestableType nil))))
      (is (= "{:id nil}" (digest-written (->OtherDigestableType nil))))
      (is (not= (digestable/sha1-hash (->DigestableType nil))
                (digestable/sha1-hash (->OtherDigestableType nil))))))

  (testing "Throws when it encounters an undigestable value."
    (is (thrown-with-msg? Exception #"Encountered undigestable value"
                          (digestable/sha1-hash (Object.))))))
