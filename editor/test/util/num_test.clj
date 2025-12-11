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

(ns util.num-test
  (:require [clojure.test :refer :all]
            [util.num :as num])
  (:import [java.nio ByteBuffer DoubleBuffer FloatBuffer LongBuffer IntBuffer ShortBuffer]))

(set! *warn-on-reflection* true)

(set! *unchecked-math* false)
(deftest unchecked-ubyte-test
  (is (= (unchecked-byte 0) (num/unchecked-ubyte 0)))
  (is (= (unchecked-byte 0) (num/unchecked-ubyte 0.9)))
  (is (= (unchecked-byte 255) (num/unchecked-ubyte 255)))
  (is (= (unchecked-byte 255) (num/unchecked-ubyte 255.9)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/unchecked-ubyte 255)) (.get 0))))
  (is (some? (num/unchecked-ubyte -1)))
  (is (some? (num/unchecked-ubyte 256)))
  (is (some? (num/unchecked-ubyte -1.5)))
  (is (some? (num/unchecked-ubyte 256.5)))
  (is (some? (mapv num/unchecked-ubyte [256.5]))))

(set! *unchecked-math* false)
(deftest unchecked-ushort-test
  (is (= (unchecked-short 0) (num/unchecked-ushort 0)))
  (is (= (unchecked-short 0) (num/unchecked-ushort 0.9)))
  (is (= (unchecked-short 65535) (num/unchecked-ushort 65535)))
  (is (= (unchecked-short 65535) (num/unchecked-ushort 65535.9)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/unchecked-ushort 65535)) (.get 0))))
  (is (some? (num/unchecked-ushort -1)))
  (is (some? (num/unchecked-ushort 65536)))
  (is (some? (num/unchecked-ushort -1.5)))
  (is (some? (num/unchecked-ushort 65536.5)))
  (is (some? (mapv num/unchecked-ushort [65536.5]))))

(set! *unchecked-math* false)
(deftest unchecked-uint-test
  (is (= (unchecked-int 0) (num/unchecked-uint 0)))
  (is (= (unchecked-int 0) (num/unchecked-uint 0.9)))
  (is (= (unchecked-int 4294967295) (num/unchecked-uint 4294967295)))
  (is (= (unchecked-int 4294967295) (num/unchecked-uint 4294967295.9)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/unchecked-uint 4294967295)) (.get 0))))
  (is (some? (num/unchecked-uint -1)))
  (is (some? (num/unchecked-uint 4294967296)))
  (is (some? (num/unchecked-uint -1.5)))
  (is (some? (num/unchecked-uint 4.2949672965E9)))
  (is (some? (mapv num/unchecked-uint [4.2949672965E9]))))

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
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: 256.5" (num/ubyte 256.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: 256.5" (mapv num/ubyte [256.5]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest ubyte-unchecked-test
  (is (= (unchecked-byte 0) (num/ubyte 0)))
  (is (= (unchecked-byte 0) (num/ubyte 0.9)))
  (is (= (unchecked-byte 255) (num/ubyte 255)))
  (is (= (unchecked-byte 255) (num/ubyte 255.9)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/ubyte 255)) (.get 0))))
  (is (some? (num/ubyte -1)))
  (is (some? (num/ubyte 256)))
  (is (some? (num/ubyte -1.5)))
  (is (some? (num/ubyte 256.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte: 256.5" (mapv num/ubyte [256.5])) "Non-inlined always checks."))

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
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: 65536.5" (num/ushort 65536.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: 65536.5" (mapv num/ushort [65536.5]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest ushort-unchecked-test
  (is (= (unchecked-short 0) (num/ushort 0)))
  (is (= (unchecked-short 0) (num/ushort 0.9)))
  (is (= (unchecked-short 65535) (num/ushort 65535)))
  (is (= (unchecked-short 65535) (num/ushort 65535.9)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/ushort 65535)) (.get 0))))
  (is (some? (num/ushort -1)))
  (is (some? (num/ushort 65536)))
  (is (some? (num/ushort -1.5)))
  (is (some? (num/ushort 65536.5)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort: 65536.5" (mapv num/ushort [65536.5])) "Non-inlined always checks."))

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
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: 4.2949672965E9" (num/uint 4.2949672965E9)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: 4.2949672965E9" (mapv num/uint [4.2949672965E9]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest uint-unchecked-test
  (is (= (unchecked-int 0) (num/uint 0)))
  (is (= (unchecked-int 0) (num/uint 0.9)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295)))
  (is (= (unchecked-int 4294967295) (num/uint 4294967295.9)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/uint 4294967295)) (.get 0))))
  (is (some? (num/uint -1)))
  (is (some? (num/uint 4294967296)))
  (is (some? (num/uint -1.5)))
  (is (some? (num/uint 4.2949672965E9)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint: 4.2949672965E9" (mapv num/uint [4.2949672965E9])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest ubyte->long-test
  (is (= 0 (num/ubyte->long (num/unchecked-ubyte 0))))
  (is (= 255 (num/ubyte->long (num/unchecked-ubyte 255))))
  (is (= 255 (-> (LongBuffer/allocate 1) (.put (num/ubyte->long (num/unchecked-ubyte 255))) (.get 0))))
  (is (= [0 255] (mapv num/ubyte->long (mapv num/unchecked-ubyte [0 255])))))

(set! *unchecked-math* false)
(deftest ushort->long-test
  (is (= 0 (num/ushort->long (num/unchecked-ushort 0))))
  (is (= 65535 (num/ushort->long (num/unchecked-ushort 65535))))
  (is (= 65535 (-> (LongBuffer/allocate 1) (.put (num/ushort->long (num/unchecked-ushort 65535))) (.get 0))))
  (is (= [0 65535] (mapv num/ushort->long (mapv num/unchecked-ushort [0 65535])))))

(set! *unchecked-math* false)
(deftest uint->long-test
  (is (= 0 (num/uint->long (num/unchecked-uint 0))))
  (is (= 4294967295 (num/uint->long (num/unchecked-uint 4294967295))))
  (is (= 4294967295 (-> (LongBuffer/allocate 1) (.put (num/uint->long (num/unchecked-uint 4294967295))) (.get 0))))
  (is (= [0 4294967295] (mapv num/uint->long (mapv num/unchecked-uint [0 4294967295])))))

(set! *unchecked-math* false)
(deftest ubyte->float-test
  (is (= (float 0.0) (num/ubyte->float (num/unchecked-ubyte 0))))
  (is (= (float 255.0) (num/ubyte->float (num/unchecked-ubyte 255))))
  (is (= (float 255.0) (-> (FloatBuffer/allocate 1) (.put (num/ubyte->float (num/unchecked-ubyte 255))) (.get 0))))
  (is (= [(float 0.0) (float 255.0)] (mapv num/ubyte->float (mapv num/unchecked-ubyte [0 255])))))

(set! *unchecked-math* false)
(deftest ushort->float-test
  (is (= (float 0.0) (num/ushort->float (num/unchecked-ushort 0))))
  (is (= (float 65535.0) (num/ushort->float (num/unchecked-ushort 65535))))
  (is (= (float 65535.0) (-> (FloatBuffer/allocate 1) (.put (num/ushort->float (num/unchecked-ushort 65535))) (.get 0))))
  (is (= [(float 0.0) (float 65535.0)] (mapv num/ushort->float (mapv num/unchecked-ushort [0 65535])))))

(set! *unchecked-math* false)
(deftest uint->float-test
  (is (= (float 0.0) (num/uint->float (num/unchecked-uint 0))))
  (is (= (float 4294967295.0) (num/uint->float (num/unchecked-uint 4294967295))))
  (is (= (float 4294967295.0) (-> (FloatBuffer/allocate 1) (.put (num/uint->float (num/unchecked-uint 4294967295))) (.get 0))))
  (is (= [(float 0.0) (float 4294967295.0)] (mapv num/uint->float (mapv num/unchecked-uint [0 4294967295])))))

(set! *unchecked-math* false)
(deftest ubyte->double-test
  (is (= 0.0 (num/ubyte->double (num/unchecked-ubyte 0))))
  (is (= 255.0 (num/ubyte->double (num/unchecked-ubyte 255))))
  (is (= 255.0 (-> (DoubleBuffer/allocate 1) (.put (num/ubyte->double (num/unchecked-ubyte 255))) (.get 0))))
  (is (= [0.0 255.0] (mapv num/ubyte->double (mapv num/unchecked-ubyte [0 255])))))

(set! *unchecked-math* false)
(deftest ushort->double-test
  (is (= 0.0 (num/ushort->double (num/unchecked-ushort 0))))
  (is (= 65535.0 (num/ushort->double (num/unchecked-ushort 65535))))
  (is (= 65535.0 (-> (DoubleBuffer/allocate 1) (.put (num/ushort->double (num/unchecked-ushort 65535))) (.get 0))))
  (is (= [0.0 65535.0] (mapv num/ushort->double (mapv num/unchecked-ushort [0 65535])))))

(set! *unchecked-math* false)
(deftest uint->double-test
  (is (= 0.0 (num/uint->double (num/unchecked-uint 0))))
  (is (= 4294967295.0 (num/uint->double (num/unchecked-uint 4294967295))))
  (is (= 4294967295.0 (-> (DoubleBuffer/allocate 1) (.put (num/uint->double (num/unchecked-uint 4294967295))) (.get 0))))
  (is (= [0.0 4294967295.0] (mapv num/uint->double (mapv num/unchecked-uint [0 4294967295])))))

(set! *unchecked-math* false)
(deftest byte-range->normalized-test
  (is (= -1.0 (num/byte-range->normalized -128.0)))
  (is (= 0.0 (num/byte-range->normalized 0.0)))
  (is (= 1.0 (num/byte-range->normalized 127.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/byte-range->normalized 127.0)) (.get 0))))
  (is (= [-1.0 1.0] (mapv num/byte-range->normalized [-128.0 127.0]))))

(set! *unchecked-math* false)
(deftest ubyte-range->normalized-test
  (is (= 0.0 (num/ubyte-range->normalized 0.0)))
  (is (= 1.0 (num/ubyte-range->normalized 255.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/ubyte-range->normalized 255.0)) (.get 0))))
  (is (= [0.0 1.0] (mapv num/ubyte-range->normalized [0.0 255]))))

(set! *unchecked-math* false)
(deftest short-range->normalized-test
  (is (= -1.0 (num/short-range->normalized -32768.0)))
  (is (= 0.0 (num/short-range->normalized 0.0)))
  (is (= 1.0 (num/short-range->normalized 32767.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/short-range->normalized 32767.0)) (.get 0))))
  (is (= [-1.0 1.0] (mapv num/short-range->normalized [-32768.0 32767.0]))))

(set! *unchecked-math* false)
(deftest ushort-range->normalized-test
  (is (= 0.0 (num/ushort-range->normalized 0.0)))
  (is (= 1.0 (num/ushort-range->normalized 65535.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/ushort-range->normalized 65535.0)) (.get 0))))
  (is (= [0.0 1.0] (mapv num/ushort-range->normalized [0.0 65535.0]))))

(set! *unchecked-math* false)
(deftest int-range->normalized-test
  (is (= -1.0 (num/int-range->normalized -2147483648.0)))
  (is (= 0.0 (num/int-range->normalized 0.0)))
  (is (= 1.0 (num/int-range->normalized 2147483647.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/int-range->normalized 2147483647.0)) (.get 0))))
  (is (= [-1.0 1.0] (mapv num/int-range->normalized [-2147483648.0 2147483647.0]))))

(set! *unchecked-math* false)
(deftest uint-range->normalized-test
  (is (= 0.0 (num/uint-range->normalized 0.0)))
  (is (= 1.0 (num/uint-range->normalized 4294967295.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/uint-range->normalized 4294967295.0)) (.get 0))))
  (is (= [0.0 1.0] (mapv num/uint-range->normalized [0.0 4294967295.0]))))

(set! *unchecked-math* false)
(deftest long-range->normalized-test
  (is (= -1.0 (num/long-range->normalized -9223372036854775808.0)))
  (is (= 0.0 (num/long-range->normalized 0.0)))
  (is (= 1.0 (num/long-range->normalized 9223372036854775807.0)))
  (is (= 1.0 (-> (DoubleBuffer/allocate 1) (.put (num/long-range->normalized 9223372036854775807.0)) (.get 0))))
  (is (= [-1.0 1.0] (mapv num/long-range->normalized [-9223372036854775808.0 9223372036854775807.0]))))

(set! *unchecked-math* false)
(deftest normalized->byte-double-test
  (is (= -128.0 (num/normalized->byte-double -1.0)))
  (is (= 0.0 (num/normalized->byte-double 0.0)))
  (is (= 127.0 (num/normalized->byte-double 1.0)))
  (is (= 127.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->byte-double 1.0)) (.get 0))))
  (is (= [-128.0 -64.0 0.0 64.0 127.0] (mapv num/normalized->byte-double [-1.0 -0.5 0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->ubyte-double-test
  (is (= 0.0 (num/normalized->ubyte-double 0.0)))
  (is (= 255.0 (num/normalized->ubyte-double 1.0)))
  (is (= 255.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->ubyte-double 1.0)) (.get 0))))
  (is (= [0.0 128.0 255.0] (mapv num/normalized->ubyte-double [0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->short-double-test
  (is (= -32768.0 (num/normalized->short-double -1.0)))
  (is (= 0.0 (num/normalized->short-double 0.0)))
  (is (= 32767.0 (num/normalized->short-double 1.0)))
  (is (= 32767.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->short-double 1.0)) (.get 0))))
  (is (= [-32768.0 -16384.0 0.0 16384.0 32767.0] (mapv num/normalized->short-double [-1.0 -0.5 0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->ushort-double-test
  (is (= 0.0 (num/normalized->ushort-double 0.0)))
  (is (= 65535.0 (num/normalized->ushort-double 1.0)))
  (is (= 65535.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->ushort-double 1.0)) (.get 0))))
  (is (= [0.0 32768.0 65535.0] (mapv num/normalized->ushort-double [0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->int-double-test
  (is (= -2147483648.0 (num/normalized->int-double -1.0)))
  (is (= 0.0 (num/normalized->int-double 0.0)))
  (is (= 2147483647.0 (num/normalized->int-double 1.0)))
  (is (= 2147483647.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->int-double 1.0)) (.get 0))))
  (is (= [-2147483648.0 -1073741824.0 0.0 1073741824.0 2147483647.0] (mapv num/normalized->int-double [-1.0 -0.5 0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->uint-double-test
  (is (= 0.0 (num/normalized->uint-double 0.0)))
  (is (= 4294967295.0 (num/normalized->uint-double 1.0)))
  (is (= 4294967295.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->uint-double 1.0)) (.get 0))))
  (is (= [0.0 2147483648.0 4294967295.0] (mapv num/normalized->uint-double [0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->long-double-test
  (is (= -9223372036854775808.0 (num/normalized->long-double -1.0)))
  (is (= 0.0 (num/normalized->long-double 0.0)))
  (is (= 9223372036854775807.0 (num/normalized->long-double 1.0)))
  (is (= 9223372036854775807.0 (-> (DoubleBuffer/allocate 1) (.put (num/normalized->long-double 1.0)) (.get 0))))
  (is (= [-9223372036854775808.0 -4611686018427387904.0 0.0 4611686018427387903.0 9223372036854775807.0] (mapv num/normalized->long-double [-1.0 -0.5 0.0 0.5 1.0]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-byte-checked-test
  (is (= (unchecked-byte -128) (num/normalized->unchecked-byte -1.0)))
  (is (= (unchecked-byte 0) (num/normalized->unchecked-byte 0.0)))
  (is (= (unchecked-byte 127) (num/normalized->unchecked-byte 1.0)))
  (is (= (unchecked-byte 127) (-> (ByteBuffer/allocate 1) (.put (num/normalized->unchecked-byte 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-byte 1.1)))
  (is (some? (num/normalized->unchecked-byte -1.1)))
  (is (some? (mapv num/normalized->unchecked-byte [-1.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-ubyte-checked-test
  (is (= (unchecked-byte 0) (num/normalized->unchecked-ubyte 0.0)))
  (is (= (unchecked-byte 255) (num/normalized->unchecked-ubyte 1.0)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/normalized->unchecked-ubyte 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-ubyte 1.1)))
  (is (some? (num/normalized->unchecked-ubyte -0.1)))
  (is (some? (mapv num/normalized->unchecked-ubyte [-0.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-short-checked-test
  (is (= (unchecked-short -32768) (num/normalized->unchecked-short -1.0)))
  (is (= (unchecked-short 0) (num/normalized->unchecked-short 0.0)))
  (is (= (unchecked-short 32767) (num/normalized->unchecked-short 1.0)))
  (is (= (unchecked-short 32767) (-> (ShortBuffer/allocate 1) (.put (num/normalized->unchecked-short 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-short 1.1)))
  (is (some? (num/normalized->unchecked-short -1.1)))
  (is (some? (mapv num/normalized->unchecked-short [-1.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-ushort-checked-test
  (is (= (unchecked-short 0) (num/normalized->unchecked-ushort 0.0)))
  (is (= (unchecked-short 65535) (num/normalized->unchecked-ushort 1.0)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/normalized->unchecked-ushort 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-ushort 1.1)))
  (is (some? (num/normalized->unchecked-ushort -0.1)))
  (is (some? (mapv num/normalized->unchecked-ushort [-0.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-int-checked-test
  (is (= (unchecked-int -2147483648) (num/normalized->unchecked-int -1.0)))
  (is (= (unchecked-int 0) (num/normalized->unchecked-int 0.0)))
  (is (= (unchecked-int 2147483647) (num/normalized->unchecked-int 1.0)))
  (is (= (unchecked-int 2147483647) (-> (IntBuffer/allocate 1) (.put (num/normalized->unchecked-int 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-int 1.1)))
  (is (some? (num/normalized->unchecked-int -1.1)))
  (is (some? (mapv num/normalized->unchecked-int [-1.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-uint-checked-test
  (is (= (unchecked-int 0) (num/normalized->unchecked-uint 0.0)))
  (is (= (unchecked-int 4294967295) (num/normalized->unchecked-uint 1.0)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/normalized->unchecked-uint 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-uint 1.1)))
  (is (some? (num/normalized->unchecked-uint -0.1)))
  (is (some? (mapv num/normalized->unchecked-uint [-0.1]))))

(set! *unchecked-math* false)
(deftest normalized->unchecked-long-checked-test
  (is (= (unchecked-long -9223372036854775808) (num/normalized->unchecked-long -1.0)))
  (is (= (unchecked-long 0) (num/normalized->unchecked-long 0.0)))
  (is (= (unchecked-long 9223372036854775807) (num/normalized->unchecked-long 1.0)))
  (is (= (unchecked-long 9223372036854775807) (-> (LongBuffer/allocate 1) (.put (num/normalized->unchecked-long 1.0)) (.get 0))))
  (is (some? (num/normalized->unchecked-long 1.1)))
  (is (some? (num/normalized->unchecked-long -1.1)))
  (is (some? (mapv num/normalized->unchecked-long [-1.1]))))

(set! *unchecked-math* false)
(deftest normalized->byte-checked-test
  (is (= (unchecked-byte -128) (num/normalized->byte -1.0)))
  (is (= (unchecked-byte 0) (num/normalized->byte 0.0)))
  (is (= (unchecked-byte 127) (num/normalized->byte 1.0)))
  (is (= (unchecked-byte 127) (-> (ByteBuffer/allocate 1) (.put (num/normalized->byte 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (num/normalized->byte 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (num/normalized->byte -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (mapv num/normalized->byte [-1.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->byte-unchecked-test
  (is (= (unchecked-byte -128) (num/normalized->byte -1.0)))
  (is (= (unchecked-byte 0) (num/normalized->byte 0.0)))
  (is (= (unchecked-byte 127) (num/normalized->byte 1.0)))
  (is (= (unchecked-byte 127) (-> (ByteBuffer/allocate 1) (.put (num/normalized->byte 1.0)) (.get 0))))
  (is (some? (num/normalized->byte 1.1)))
  (is (some? (num/normalized->byte -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for byte" (mapv num/normalized->byte [-1.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->ubyte-checked-test
  (is (= (unchecked-byte 0) (num/normalized->ubyte 0.0)))
  (is (= (unchecked-byte 255) (num/normalized->ubyte 1.0)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/normalized->ubyte 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (num/normalized->ubyte 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (num/normalized->ubyte -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (mapv num/normalized->ubyte [-0.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->ubyte-unchecked-test
  (is (= (unchecked-byte 0) (num/normalized->ubyte 0.0)))
  (is (= (unchecked-byte 255) (num/normalized->ubyte 1.0)))
  (is (= (unchecked-byte 255) (-> (ByteBuffer/allocate 1) (.put (num/normalized->ubyte 1.0)) (.get 0))))
  (is (some? (num/normalized->ubyte 1.1)))
  (is (some? (num/normalized->ubyte -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ubyte" (mapv num/normalized->ubyte [-0.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->short-checked-test
  (is (= (unchecked-short -32768) (num/normalized->short -1.0)))
  (is (= (unchecked-short 0) (num/normalized->short 0.0)))
  (is (= (unchecked-short 32767) (num/normalized->short 1.0)))
  (is (= (unchecked-short 32767) (-> (ShortBuffer/allocate 1) (.put (num/normalized->short 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (num/normalized->short 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (num/normalized->short -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (mapv num/normalized->short [-1.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->short-unchecked-test
  (is (= (unchecked-short -32768) (num/normalized->short -1.0)))
  (is (= (unchecked-short 0) (num/normalized->short 0.0)))
  (is (= (unchecked-short 32767) (num/normalized->short 1.0)))
  (is (= (unchecked-short 32767) (-> (ShortBuffer/allocate 1) (.put (num/normalized->short 1.0)) (.get 0))))
  (is (some? (num/normalized->short 1.1)))
  (is (some? (num/normalized->short -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for short" (mapv num/normalized->short [-1.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->ushort-checked-test
  (is (= (unchecked-short 0) (num/normalized->ushort 0.0)))
  (is (= (unchecked-short 65535) (num/normalized->ushort 1.0)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/normalized->ushort 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (num/normalized->ushort 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (num/normalized->ushort -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (mapv num/normalized->ushort [-0.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->ushort-unchecked-test
  (is (= (unchecked-short 0) (num/normalized->ushort 0.0)))
  (is (= (unchecked-short 65535) (num/normalized->ushort 1.0)))
  (is (= (unchecked-short 65535) (-> (ShortBuffer/allocate 1) (.put (num/normalized->ushort 1.0)) (.get 0))))
  (is (some? (num/normalized->ushort 1.1)))
  (is (some? (num/normalized->ushort -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for ushort" (mapv num/normalized->ushort [-0.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->int-checked-test
  (is (= (unchecked-int -2147483648) (num/normalized->int -1.0)))
  (is (= (unchecked-int 0) (num/normalized->int 0.0)))
  (is (= (unchecked-int 2147483647) (num/normalized->int 1.0)))
  (is (= (unchecked-int 2147483647) (-> (IntBuffer/allocate 1) (.put (num/normalized->int 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (num/normalized->int 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (num/normalized->int -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (mapv num/normalized->int [-1.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->int-unchecked-test
  (is (= (unchecked-int -2147483648) (num/normalized->int -1.0)))
  (is (= (unchecked-int 0) (num/normalized->int 0.0)))
  (is (= (unchecked-int 2147483647) (num/normalized->int 1.0)))
  (is (= (unchecked-int 2147483647) (-> (IntBuffer/allocate 1) (.put (num/normalized->int 1.0)) (.get 0))))
  (is (some? (num/normalized->int 1.1)))
  (is (some? (num/normalized->int -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for int" (mapv num/normalized->int [-1.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->uint-checked-test
  (is (= (unchecked-int 0) (num/normalized->uint 0.0)))
  (is (= (unchecked-int 4294967295) (num/normalized->uint 1.0)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/normalized->uint 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (num/normalized->uint 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (num/normalized->uint -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (mapv num/normalized->uint [-0.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->uint-unchecked-test
  (is (= (unchecked-int 0) (num/normalized->uint 0.0)))
  (is (= (unchecked-int 4294967295) (num/normalized->uint 1.0)))
  (is (= (unchecked-int 4294967295) (-> (IntBuffer/allocate 1) (.put (num/normalized->uint 1.0)) (.get 0))))
  (is (some? (num/normalized->uint 1.1)))
  (is (some? (num/normalized->uint -0.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for uint" (mapv num/normalized->uint [-0.1])) "Non-inlined always checks."))

(set! *unchecked-math* false)
(deftest normalized->long-checked-test
  (is (= (unchecked-long -9223372036854775808) (num/normalized->long -1.0)))
  (is (= (unchecked-long 0) (num/normalized->long 0.0)))
  (is (= (unchecked-long 9223372036854775807) (num/normalized->long 1.0)))
  (is (= (unchecked-long 9223372036854775807) (-> (LongBuffer/allocate 1) (.put (num/normalized->long 1.0)) (.get 0))))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for long" (num/normalized->long 1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for long" (num/normalized->long -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for long" (mapv num/normalized->long [-1.1]))))

(set! *unchecked-math* :warn-on-boxed)
(deftest normalized->long-unchecked-test
  (is (= (unchecked-long -9223372036854775808) (num/normalized->long -1.0)))
  (is (= (unchecked-long 0) (num/normalized->long 0.0)))
  (is (= (unchecked-long 9223372036854775807) (num/normalized->long 1.0)))
  (is (= (unchecked-long 9223372036854775807) (-> (LongBuffer/allocate 1) (.put (num/normalized->long 1.0)) (.get 0))))
  (is (some? (num/normalized->long 1.1)))
  (is (some? (num/normalized->long -1.1)))
  (is (thrown-with-msg? IllegalArgumentException #"Value out of range for long" (mapv num/normalized->long [-1.1])) "Non-inlined always checks."))
