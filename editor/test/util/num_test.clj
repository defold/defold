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

(ns util.num-test
  (:require [clojure.test :refer :all]
            [util.num :as num])
  (:import [java.nio ByteBuffer IntBuffer ShortBuffer]))

(set! *warn-on-reflection* true)

(set! *unchecked-math* false)
(deftest ubyte-checked-test
  (is (= (unchecked-byte 0) (num/ubyte 0)))
  (is (= (unchecked-byte 0) (num/ubyte 0.9)))
  (is (= (unchecked-byte 255) (num/ubyte 255)))
  (is (= (unchecked-byte 255) (num/ubyte 255.9)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/ubyte 255)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: -1" (num/ubyte -1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: 256" (num/ubyte 256)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: -1.5" (num/ubyte -1.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: 256.5" (num/ubyte 256.5))))

(set! *unchecked-math* :warn-on-boxed)
(deftest ubyte-unchecked-test
  (is (= (unchecked-byte 0) (num/ubyte 0)))
  (is (= (unchecked-byte 0) (num/ubyte 0.9)))
  (is (= (unchecked-byte 255) (num/ubyte 255)))
  (is (= (unchecked-byte 255) (num/ubyte 255.9)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/ubyte 255)) (.get 0))))
  (is (num/ubyte -1))
  (is (num/ubyte 256))
  (is (num/ubyte -1.5))
  (is (num/ubyte 256.5)))

(set! *unchecked-math* false)
(deftest ushort-checked-test
  (is (= (unchecked-short 0) (num/ushort 0)))
  (is (= (unchecked-short 0) (num/ushort 0.9)))
  (is (= (unchecked-short 65535) (num/ushort 65535)))
  (is (= (unchecked-short 65535) (num/ushort 65535.9)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/ushort 65535)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: -1" (num/ushort -1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: 65536" (num/ushort 65536)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: -1.5" (num/ushort -1.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: 65536.5" (num/ushort 65536.5))))

(set! *unchecked-math* :warn-on-boxed)
(deftest ushort-unchecked-test
  (is (= (unchecked-short 0) (num/ushort 0)))
  (is (= (unchecked-short 0) (num/ushort 0.9)))
  (is (= (unchecked-short 65535) (num/ushort 65535)))
  (is (= (unchecked-short 65535) (num/ushort 65535.9)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/ushort 65535)) (.get 0))))
  (is (num/ushort -1))
  (is (num/ushort 65536))
  (is (num/ushort -1.5))
  (is (num/ushort 65536.5)))

(set! *unchecked-math* false)
(deftest uint-checked-test
  (is (= (unchecked-int 0) (num/uint 0)))
  (is (= (unchecked-int 0) (num/uint 0.9)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295.9)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/uint 4294967295)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: -1" (num/uint -1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: 4294967296" (num/uint 4294967296)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: -1.5" (num/uint -1.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: 4.2949672965E9" (num/uint 4.2949672965E9))))

(set! *unchecked-math* :warn-on-boxed)
(deftest uint-unchecked-test
  (is (= (unchecked-int 0) (num/uint 0)))
  (is (= (unchecked-int 0) (num/uint 0.9)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295.9)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/uint 4294967295)) (.get 0))))
  (is (num/uint -1))
  (is (num/uint 4294967296))
  (is (num/uint -1.5))
  (is (num/uint 4.2949672965E9)))

(set! *unchecked-math* false)
(deftest normalized->byte-checked-test
  (is (= (unchecked-byte -128) (num/normalized->byte -1.0)))
  (is (= (unchecked-byte 0) (num/normalized->byte 0.0)))
  (is (= (unchecked-byte 127) (num/normalized->byte 1.0)))
  (is (= (unchecked-byte 127) (-> (ByteBuffer/allocate 1) (.put (num/normalized->byte 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (num/normalized->byte 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (num/normalized->byte -1.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->byte-unchecked-test
  (is (= (unchecked-byte -128) (num/normalized->byte -1.0)))
  (is (= (unchecked-byte 0) (num/normalized->byte 0.0)))
  (is (= (unchecked-byte 127) (num/normalized->byte 1.0)))
  (is (= (unchecked-byte 127) (-> (ByteBuffer/allocate 1) (.put (num/normalized->byte 1.0)) (.get 0))))
  (is (num/normalized->byte 1.1))
  (is (num/normalized->byte -1.1)))

(set! *unchecked-math* false)
(deftest normalized->ubyte-checked-test
  (is (= (unchecked-byte 0) (num/normalized->ubyte 0.0)))
  (is (= (unchecked-byte 255) (num/normalized->ubyte 1.0)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/normalized->ubyte 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (num/normalized->ubyte 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (num/normalized->ubyte -0.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->ubyte-unchecked-test
  (is (= (unchecked-byte 0) (num/normalized->ubyte 0.0)))
  (is (= (unchecked-byte 255) (num/normalized->ubyte 1.0)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/normalized->ubyte 1.0)) (.get 0))))
  (is (num/normalized->ubyte 1.1))
  (is (num/normalized->ubyte -0.1)))

(set! *unchecked-math* false)
(deftest normalized->short-checked-test
  (is (= (unchecked-short -32768) (num/normalized->short -1.0)))
  (is (= (unchecked-short 0) (num/normalized->short 0.0)))
  (is (= (unchecked-short 32767) (num/normalized->short 1.0)))
  (is (= (unchecked-short 32767) (-> (ShortBuffer/allocate 1) (.put (num/normalized->short 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (num/normalized->short 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (num/normalized->short -1.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->short-unchecked-test
  (is (= (unchecked-short -32768) (num/normalized->short -1.0)))
  (is (= (unchecked-short 0) (num/normalized->short 0.0)))
  (is (= (unchecked-short 32767) (num/normalized->short 1.0)))
  (is (= (unchecked-short 32767) (-> (ShortBuffer/allocate 1) (.put (num/normalized->short 1.0)) (.get 0))))
  (is (num/normalized->short 1.1))
  (is (num/normalized->short -1.1)))

(set! *unchecked-math* false)
(deftest normalized->ushort-checked-test
  (is (= (unchecked-short 0) (num/normalized->ushort 0.0)))
  (is (= (unchecked-short 65535) (num/normalized->ushort 1.0)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/normalized->ushort 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (num/normalized->ushort 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (num/normalized->ushort -0.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->ushort-unchecked-test
  (is (= (unchecked-short 0) (num/normalized->ushort 0.0)))
  (is (= (unchecked-short 65535) (num/normalized->ushort 1.0)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/normalized->ushort 1.0)) (.get 0))))
  (is (num/normalized->ushort 1.1))
  (is (num/normalized->ushort -0.1)))

(set! *unchecked-math* false)
(deftest normalized->int-checked-test
  (is (= (unchecked-int -2147483648) (num/normalized->int -1.0)))
  (is (= (unchecked-int 0) (num/normalized->int 0.0)))
  (is (= (unchecked-int 2147483647) (num/normalized->int 1.0)))
  (is (= (unchecked-int 2147483647) (-> (IntBuffer/allocate 1) (.put (num/normalized->int 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (num/normalized->int 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (num/normalized->int -1.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->int-unchecked-test
  (is (= (unchecked-int -2147483648) (num/normalized->int -1.0)))
  (is (= (unchecked-int 0) (num/normalized->int 0.0)))
  (is (= (unchecked-int 2147483647) (num/normalized->int 1.0)))
  (is (= (unchecked-int 2147483647) (-> (IntBuffer/allocate 1) (.put (num/normalized->int 1.0)) (.get 0))))
  (is (num/normalized->int 1.1))
  (is (num/normalized->int -1.1)))

(set! *unchecked-math* false)
(deftest normalized->uint-checked-test
  (is (= (unchecked-int 0) (num/normalized->uint 0.0)))
  (is (= (unchecked-int 4294967295) (num/normalized->uint 1.0)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/normalized->uint 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (num/normalized->uint 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (num/normalized->uint -0.1))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->uint-unchecked-test
  (is (= (unchecked-int 0) (num/normalized->uint 0.0)))
  (is (= (unchecked-int 4294967295) (num/normalized->uint 1.0)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/normalized->uint 1.0)) (.get 0))))
  (is (num/normalized->uint 1.1))
  (is (num/normalized->uint -0.1)))
