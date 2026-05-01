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

(ns editor.field-expression-test
  (:require [clojure.test :refer :all]
            [editor.field-expression :as field-expression :refer [format-number to-double to-float to-int to-long]]))

(deftest scientific->decimal-test
  (let [scientific->decimal #'field-expression/scientific->decimal]
    (is (= "12300000.0" (scientific->decimal "123.0E5")))
    (is (= "1230000.0" (scientific->decimal "123.0E4")))
    (is (= "123000.0" (scientific->decimal "123.0E3")))
    (is (= "12300.0" (scientific->decimal "123.0E2")))
    (is (= "1230.0" (scientific->decimal "123.0E1")))
    (is (= "123.0" (scientific->decimal "123.0")))
    (is (= "12.3" (scientific->decimal "123.0E-1")))
    (is (= "1.23" (scientific->decimal "123.0E-2")))
    (is (= "0.123" (scientific->decimal "123.0E-3")))
    (is (= "0.0123" (scientific->decimal "123.0E-4")))
    (is (= "0.00123" (scientific->decimal "123.0E-5")))

    (is (= "12340000.0" (scientific->decimal "123.4E5")))
    (is (= "1234000.0" (scientific->decimal "123.4E4")))
    (is (= "123400.0" (scientific->decimal "123.4E3")))
    (is (= "12340.0" (scientific->decimal "123.4E2")))
    (is (= "1234.0" (scientific->decimal "123.4E1")))
    (is (= "123.4" (scientific->decimal "123.4")))
    (is (= "12.34" (scientific->decimal "123.4E-1")))
    (is (= "1.234" (scientific->decimal "123.4E-2")))
    (is (= "0.1234" (scientific->decimal "123.4E-3")))
    (is (= "0.01234" (scientific->decimal "123.4E-4")))
    (is (= "0.001234" (scientific->decimal "123.4E-5")))

    (is (= "12345000.0" (scientific->decimal "123.45E5")))
    (is (= "1234500.0" (scientific->decimal "123.45E4")))
    (is (= "123450.0" (scientific->decimal "123.45E3")))
    (is (= "12345.0" (scientific->decimal "123.45E2")))
    (is (= "1234.5" (scientific->decimal "123.45E1")))
    (is (= "123.45" (scientific->decimal "123.45")))
    (is (= "12.345" (scientific->decimal "123.45E-1")))
    (is (= "1.2345" (scientific->decimal "123.45E-2")))
    (is (= "0.12345" (scientific->decimal "123.45E-3")))
    (is (= "0.012345" (scientific->decimal "123.45E-4")))
    (is (= "0.0012345" (scientific->decimal "123.45E-5")))

    (is (= "-12345000.0" (scientific->decimal "-123.45E5")))
    (is (= "-1234500.0" (scientific->decimal "-123.45E4")))
    (is (= "-123450.0" (scientific->decimal "-123.45E3")))
    (is (= "-12345.0" (scientific->decimal "-123.45E2")))
    (is (= "-1234.5" (scientific->decimal "-123.45E1")))
    (is (= "-123.45" (scientific->decimal "-123.45")))
    (is (= "-12.345" (scientific->decimal "-123.45E-1")))
    (is (= "-1.2345" (scientific->decimal "-123.45E-2")))
    (is (= "-0.12345" (scientific->decimal "-123.45E-3")))
    (is (= "-0.012345" (scientific->decimal "-123.45E-4")))
    (is (= "-0.0012345" (scientific->decimal "-123.45E-5")))))

(deftest format-number-double-test
  (are [n s]
    (= s (format-number (double n)))

    Double/NaN "NaN"
    Double/POSITIVE_INFINITY "Infinity"
    Double/NEGATIVE_INFINITY "-Infinity"
    0 "0.0"
    -1 "-1.0"
    0.1 "0.1"
    0.00001 "0.00001"
    1.000001 "1.000001"
    10000000.0 "10000000.0"
    1100.11 "1100.11"
    233.21419 "233.21419"
    -233.21419 "-233.21419"

    3.141592653589793 "3.141592653589793"
    31.41592653589793 "31.41592653589793"
    314.1592653589793 "314.1592653589793"
    3141.592653589793 "3141.592653589793"
    31415.92653589793 "31415.92653589793"
    314159.2653589793 "314159.2653589793"
    3141592.653589793 "3141592.653589793"
    31415926.53589793 "31415926.53589793"
    314159265.3589793 "314159265.3589793"
    3141592653.589793 "3141592653.589793"
    31415926535.89793 "31415926535.89793"
    314159265358.9793 "314159265358.9793"
    3141592653589.793 "3141592653589.793"
    31415926535897.93 "31415926535897.93"
    314159265358979.3 "314159265358979.3"))

(deftest format-number-float-test
  (are [n s]
    (= s (format-number (float n)))

    Float/NaN "NaN"
    Float/POSITIVE_INFINITY "Infinity"
    Float/NEGATIVE_INFINITY "-Infinity"
    0 "0.0"
    -1 "-1.0"
    0.1 "0.1"
    0.00001 "0.00001"
    1.000001 "1.000001"
    10000000.0 "10000000.0"
    1100.11 "1100.11"
    233.21419 "233.21419"
    -233.21419 "-233.21419"

    3.141593 "3.141593"
    31.41593 "31.41593"
    314.1593 "314.1593"
    3141.593 "3141.593"
    31415.93 "31415.93"
    314159.3 "314159.3"))

(deftest format-number-int-test
  (are [n s]
    (= s (format-number (int n)))

    0 "0"
    -1 "-1"
    2147483647 "2147483647"
    -2147483648 "-2147483648"))

(deftest format-number-long-test
  (are [n s]
    (= s (format-number (long n)))

    0 "0"
    -1 "-1"
    9223372036854775807 "9223372036854775807"
    -9223372036854775808 "-9223372036854775808"))

(deftest format-number-test
  (are [n s]
    (= s (format-number n))

    nil ""
    Double/NaN "NaN"
    Double/POSITIVE_INFINITY "Infinity"
    Double/NEGATIVE_INFINITY "-Infinity"
    Float/NaN "NaN"
    Float/POSITIVE_INFINITY "Infinity"
    Float/NEGATIVE_INFINITY "-Infinity"
    0 "0"
    -1 "-1"
    32767 "32767"
    -32768 "-32768"
    8388607 "8388607"
    -8388608 "-8388608"
    2147483647 "2147483647"
    -2147483648 "-2147483648"
    9223372036854775807 "9223372036854775807"
    -9223372036854775808 "-9223372036854775808"
    (float 0.1) "0.1"
    (float 0.00001) "0.00001"
    (float 1.000001) "1.000001"
    (float 10000000.0) "10000000.0"
    (float 1100.11) "1100.11"
    (float 233.21419) "233.21419"
    233.21419 "233.21419"
    3.141592653589793 "3.141592653589793"))

(deftest to-double-test
  (testing "String conversion"
    (is (= 0.0 (to-double "0")))
    (is (= 1.1 (to-double "1.1")))
    (is (= -2.0 (to-double "-2")))
    (is (= -2.5 (to-double "-2.5")))
    (is (= 3.1 (to-double "+3.1")))
    (is (= 0.0001 (to-double "1.0e-4")))
    (is (= 0.00001 (to-double "1.0E-5")))
    (is (= 100000000000000000000.0 (to-double "1.0E20")))
    (is (= 1000000000000000000000.0 (to-double "1.0E+21"))))

  (testing "Arithmetic"
    (testing "Whitespace not significant"
      (is (= 3.0 (to-double "1+2")))
      (is (= 4.0 (to-double "2 + 2"))))

    (testing "Basic operations"
      (is (= 20.0 (to-double "10 + 10")))
      (is (= 10.0 (to-double "20 - 10")))
      (is (= 25.0 (to-double "5 * 5")))
      (is (= 30.0 (to-double "60 / 2"))))

    (testing "Scientific notation terms"
      (is (= 1.0 (to-double "1.0e-4 / 1.0e-4")))
      (is (= 1.0 (to-double "1.0E-4 / 1.0E-4"))))))

(deftest to-int-test
  (testing "String conversion"
    (is (= 0 (to-int "0")))
    (is (= 1 (to-int "1")))
    (is (= -2 (to-int "-2")))
    (is (= 3 (to-int "+3"))))

  (testing "Arithmetic"
    (testing "Whitespace not significant"
      (is (= 3 (to-int "1+2")))
      (is (= 4 (to-int "2 + 2"))))

    (testing "Basic operations"
      (is (= 20 (to-int "10 + 10")))
      (is (= 10 (to-int "20 - 10")))
      (is (= 25 (to-int "5 * 5")))
      (is (= 30 (to-int "60 / 2"))))))

(deftest double-loopback-test
  (is (true? (Double/isNaN (to-double (format-number Double/NaN)))))
  (are [n]
    (= n (to-double (format-number n)))

    Double/MAX_VALUE
    Double/MIN_VALUE
    Double/MIN_NORMAL
    Double/POSITIVE_INFINITY
    Double/NEGATIVE_INFINITY))

(deftest float-loopback-test
  (is (true? (Float/isNaN (to-float (format-number Float/NaN)))))
  (are [n]
    (= n (to-float (format-number n)))

    Float/MAX_VALUE
    Float/MIN_VALUE
    Float/MIN_NORMAL
    Float/POSITIVE_INFINITY
    Float/NEGATIVE_INFINITY))

(deftest int-loopback-test
  (are [n]
    (= n (to-int (format-number n)))

    Integer/MAX_VALUE
    Integer/MIN_VALUE))

(deftest long-loopback-test
  (are [n]
    (= n (to-long (format-number n)))

    Long/MAX_VALUE
    Long/MIN_VALUE))

(deftest number-loopback-test
  (is (true? (Double/isNaN (to-double (format-number Double/NaN)))))
  (are [n]
    (= n (to-double (format-number n)))

    Double/MAX_VALUE
    Double/MIN_VALUE
    Double/MIN_NORMAL
    Double/POSITIVE_INFINITY
    Double/NEGATIVE_INFINITY

    0.0
    -1.0
    0.1
    0.00001
    1.000001
    1100.11
    9223372036854775807.0
    -9223372036854775808.0

    3.141592653589793
    31.41592653589793
    314.1592653589793
    3141.592653589793
    31415.92653589793
    314159.2653589793
    3141592.653589793
    31415926.53589793
    314159265.3589793
    3141592653.589793
    31415926535.89793
    314159265358.9793
    3141592653589.793
    31415926535897.93
    314159265358979.3)

  (is (true? (Double/isNaN (to-double (format-number Float/NaN)))))
  (is (= Double/POSITIVE_INFINITY (to-double (format-number Float/POSITIVE_INFINITY))))
  (is (= Double/NEGATIVE_INFINITY (to-double (format-number Float/NEGATIVE_INFINITY))))
  (are [n]
    (= n (to-double (format-number (float n))))

    0.0
    -1.0
    0.1
    0.00001
    1.000001
    1100.11
    32767.0
    -32768.0
    8388607.0
    -8388608.0

    3.141593
    31.41593
    314.1593
    3141.593
    31415.93
    314159.3))
