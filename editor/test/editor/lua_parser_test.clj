(ns editor.lua-parser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.lua-parser :refer :all])
  (:import [org.apache.commons.lang RandomStringUtils]))

(deftest test-variables
  (testing "global variable with assignment"
    (is (= #{"foo"} (:vars (lua-info "foo=1")))))
  (testing "mulitiple global variable with assignment"
    (is (= #{"foo" "bar"} (:vars (lua-info "foo,bar=1,2")))))
  (testing "one local with no assignment"
    (is (= #{"foo"} (:local-vars (lua-info "local foo")))))
  (testing "multiple locals with no assignment"
    (is (= #{"foo" "bar"} (:local-vars (lua-info "local foo,bar")))))
  (testing "one local with assignment"
    (is (= #{"foo"} (:local-vars (lua-info "local foo=1")))))
  (testing "multiple locals with assignment"
    (is (= #{"foo" "bar"} (:local-vars (lua-info "local foo,bar = 1,3")))))
  (testing "local with a require assignment"
    (let [code "local mymathmodule = require(\"mymath\")"
          result (select-keys (lua-info code) [:local-vars :requires])]
      (is (= {:local-vars #{"mymathmodule"}
              :requires [["mymathmodule" "mymath"]]} result))))
  (testing "global with a require assignment"
    (let [code "mymathmodule = require(\"mymath\")"
          result (select-keys (lua-info code) [:vars :requires])]
      (is (= {:vars #{"mymathmodule"}
              :requires [["mymathmodule" "mymath"]]} result))))
  (testing "multiple local require assignments"
    (let [code "local x = require(\"myx\") \n local y = require(\"myy\")"
          result (select-keys (lua-info code) [:local-vars :requires])]
      (is (= {:local-vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "local with multiple require assignments"
    (let [code "local x,y = require(\"myx\"), require(\"myy\")"
          result (select-keys (lua-info code) [:local-vars :requires])]
      (is (= {:local-vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "multiple global require assignments"
    (let [code "x = require(\"myx\") \n y = require(\"myy\")"
          result (select-keys (lua-info code) [:vars :requires])]
      (is (= {:vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "multiple require string types"
    (let [code "x = require('myx') \n y = require(\"myy\") \n z = require([==[myz]==])"
          result (select-keys (lua-info code) [:vars :requires])]
      (is (= {:vars #{"x" "y" "z"}
              :requires [["x" "myx"] ["y" "myy"] ["z" "myz"]]} result))))
  (testing "global with multiple require assignments"
    (let [code "x,y = require(\"myx\"), require(\"myy\")"
          result (select-keys (lua-info code) [:vars :requires])]
      (is (= {:vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result)))))

(deftest test-require
  (testing "bare require function call"
    (let [code "require(\"mymath\")"
          result (select-keys (lua-info code) [:vars :requires])]
      (is (= {:vars #{}
              :requires [[nil "mymath"]]} result)))))

(deftest test-functions
  (testing "global function with no params"
    (let [code "function oadd() return num1 end"]
      (is (= {"oadd" {:params []}} (:functions (lua-info code))))))
  (testing "local function with no params"
    (let [code "local function oadd() return num1 end"]
      (is (= {"oadd" {:params []}} (:local-functions (lua-info code))))))
  (testing "global function with namespace"
    (let [code "function foo.oadd() return num1 end"]
      (is (= {"foo.oadd" {:params []}} (:functions (lua-info code))))))
  (testing "global function with params"
    (let [code "function oadd(num1, num2) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}} (:functions (lua-info code))))))
  (testing "global multiple functions"
    (let [code "function oadd(num1, num2) return num1 end \n function tadd(foo) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}, "tadd" {:params ["foo"]}} (:functions (lua-info code)))))))

(deftest test-lua-info-with-files
  (testing "variable test file"
    (let [result (lua-info (slurp (io/resource "lua/variable_test.lua")))]
      (is (= {:local-vars #{"a" "b" "c" "d"}
              :vars #{"x"}
              :functions {}
              :local-functions {}
              :requires []} result))))
  (testing "function test file"
    (let [result (lua-info (slurp (io/resource "lua/function_test.lua")))]
      (is (= {:vars #{}
              :local-vars #{}
              :local-functions {"ladd" {:params ["num1" "num2"]}}
              :functions {"add" {:params ["num1" "num2"]}
                          "oadd" {:params ["num1" "num2"]}}
              :requires []} result))))
  (testing "require test file"
    (let [result (lua-info (slurp (io/resource "lua/require_test.lua")))]
      (is (= {:vars #{"foo"}
              :local-vars #{"bar"}
              :functions {}
              :local-functions {}
              :requires [[nil "mymath"] ["foo" "mymath"] ["bar" "mymath"]]} result)))))

(deftest test-lua-info-with-spaceship
  (let [result (lua-info (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script")))]
    (is (= {:vars #{"self"}
            :local-vars #{"max_speed" "min_y" "max_y" "p"}
            :functions {"init" {:params ["self"]}
                        "update" {:params ["self" "dt"]}
                        "on_input" {:params ["self" "action_id" "action"]}}
            :local-functions {}
            :requires []} result))))

(deftest test-lua-info-with-props-script
  (let [result (lua-info (slurp (io/resource "build_project/SideScroller/script/props.script")))]
    (is (= {:vars #{}
            :local-vars #{}
            :functions {}
            :local-functions {}
            :requires [[nil "script.test"]]} result))))

(defn- soil-once [code]
  (let [split (rand-int (count code))
        before (subs code 0 split)
        dirt (RandomStringUtils/random (rand-int 5) ".+-,;|!<>")
        after (subs code (+ split (rand-int (- (count code) split))))]
    (str before dirt after)))

(defn- soil [code]
  (nth (iterate soil-once code) 5))

(def last-fuzz (atom nil))
(def last-broken (atom nil))

(deftest parsing-garbage-works
  (let [fuzz-codes (take 100 (repeatedly #(RandomStringUtils/random (rand-int 400))))]
    (doall (map #(do (reset! last-fuzz %) (lua-info %)) fuzz-codes))

  (let [original (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script"))
        broken-versions (map soil (repeat 100 original))]
    (doall (map #(do (reset! last-broken %) (lua-info %)) broken-versions)))))

(deftest string-as-function-parameter-is-ignored
  (let [code "function fun(\"foo\") end"
        result (lua-info code)]
    (is (= {:vars #{}
            :local-vars #{}
            :functions {"fun" {:params []}}
            :local-functions {}
            :requires []} result))))

