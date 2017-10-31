(ns editor.field-expression-test
  (:require [clojure.test :refer :all]
            [editor.field-expression :refer [format-double format-int to-double to-int]]))

(deftest format-double-test
  (are [n s]
    (= s (format-double n))

    nil ""
    0 "0"
    -1 "-1"
    0.1 "0.1"
    0.00001 "0.00001"
    1.000001 "1.000001"
    1100.11 "1100.11"
    9223372036854775807 "9223372036854775807"
    -9223372036854775808 "-9223372036854775808"

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

(deftest format-int-test
  (are [n s]
    (= s (format-int n))

    nil ""
    0 "0"
    -1 "-1"
    2147483647 "2147483647"
    -2147483648 "-2147483648"))

(deftest to-double-test
  (testing "String conversion"
    (is (= 0.0 (to-double "0")))
    (is (= 1.1 (to-double "1.1")))
    (is (= -2.0 (to-double "-2")))
    (is (= -2.5 (to-double "-2.5")))
    (is (= -1.5 (to-double "-1,5")))
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
  (is (true? (Double/isNaN (to-double (format-double Double/NaN)))))
  (are [n]
    (= n (to-double (format-double n)))

    Double/MAX_VALUE
    Double/MIN_VALUE
    Double/MIN_NORMAL
    Double/POSITIVE_INFINITY
    Double/NEGATIVE_INFINITY))

(deftest int-loopback-test
  (are [n]
    (= n (to-int (format-int n)))

    Integer/MAX_VALUE
    Integer/MIN_VALUE))
