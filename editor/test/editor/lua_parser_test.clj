(ns editor.lua-parser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.lua-parser :refer :all]))

(deftest test-variables
  (testing "global variable"
    (is (= ["foo"] (:locals (lua-info "foo")))))
  (testing "one local with no assignment"
    (is (= ["foo"] (:locals (lua-info "local foo")))))
  (testing "multiple locals with no assignment"
    (is (= ["foo" "bar"] (:locals (lua-info "local foo,bar")))))
  (testing "one local with assignment"
    (is (= ["foo"] (:locals (lua-info "local foo=1")))))
  (testing "multiple locals with assignment"
    (is (= ["foo" "bar"] (:locals (lua-info "local foo,bar = 1,3")))))
  (testing "local with a require assignment"
    (let [code "local mymathmodule = require(\"mymath\")"
          result (select-keys (lua-info code) [:locals :requires])]
     (is (= {:locals ["mymathmodule"]
             :requires {"mymathmodule" "mymath"}} result))))
  (testing "local with multiple require assignments"
    (let [code "local x = require(\"myx\") \n local y = require(\"myy\")"
          result (select-keys (lua-info code) [:locals :requires])]
     (is (= {:locals ["x" "y"]
             :requires {"x" "myx" "y" "myy"}} result)))))

(deftest test-functions
  (testing "one function with no params"
    (let [code "function oadd() return num1 end"]
      (is (= {"oadd" {:params []}} (:functions (lua-info code))))))
  (testing "one function with params"
    (let [code "function oadd(num1, num2) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}} (:functions (lua-info code))))))
  (testing "multiple functions"
    (let [code "function oadd(num1, num2) return num1 end \n function tadd(foo) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}, "tadd" {:params ["foo"]}} (:functions (lua-info code)))))))


(deftest test-lua-info-with-files
  (testing "variable test file"
    (let [result (lua-info (slurp (io/resource "lua/variable_test.lua")))]
      (is (= {:locals ["a" "b" "c" "d"]
              :functions {}
              :requires {}} result))))
(testing "function test file"
    (let [result (lua-info (slurp (io/resource "lua/function_test.lua")))]
      (is (= {:locals []
              :functions {"add" {:params ["num1" "num2"]}
                          "ladd" {:params ["num1" "num2"]}
                          "oadd" {:params ["num1" "num2"]}}
              :requires {}} result))))
  )
