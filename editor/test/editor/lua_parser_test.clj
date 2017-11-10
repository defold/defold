(ns editor.lua-parser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.lua-parser :refer :all]
            [clojure.string :as string])
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
              :script-properties []
              :requires []} result))))
  (testing "function test file"
    (let [result (lua-info (slurp (io/resource "lua/function_test.lua")))]
      (is (= {:vars #{}
              :local-vars #{}
              :local-functions {"ladd" {:params ["num1" "num2"]}}
              :functions {"add" {:params ["num1" "num2"]}
                          "oadd" {:params ["num1" "num2"]}}
              :script-properties []
              :requires []} result))))
  (testing "require test file"
    (let [result (lua-info (slurp (io/resource "lua/require_test.lua")))]
      (is (= {:vars #{"foo"}
              :local-vars #{"bar"}
              :functions {}
              :local-functions {}
              :script-properties []
              :requires [[nil "mymath"] ["foo" "mymath"] ["bar" "mymath"]]} result)))))

(deftest test-lua-info-with-spaceship
  (let [result (lua-info (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script")))]
    (is (= {:vars #{"self"}
            :local-vars #{"max_speed" "min_y" "max_y" "p"}
            :functions {"init" {:params ["self"]}
                        "update" {:params ["self" "dt"]}
                        "on_input" {:params ["self" "action_id" "action"]}}
            :local-functions {}
            :script-properties []
            :requires []} result))))

(deftest test-lua-info-with-props-script
  (let [result (lua-info (slurp (io/resource "build_project/SideScroller/script/props.script")))]
    (is (= {:vars #{}
            :local-vars #{}
            :functions {}
            :local-functions {}
            :script-properties [{:name "number"
                                 :type :property-type-number
                                 :value 100.0
                                 :status :ok}
                                {:name "number1"
                                 :type :property-type-number
                                 :value 1.0
                                 :status :ok}
                                {:name "number2"
                                 :type :property-type-number
                                 :value 0.1
                                 :status :ok}
                                {:name "number3"
                                 :type :property-type-number
                                 :value -0.1
                                 :status :ok}
                                {:name "url"
                                 :type :property-type-url
                                 :value "url"
                                 :status :ok}
                                {:name "url1"
                                 :type :property-type-url
                                 :value ""
                                 :status :ok}
                                {:name "url2"
                                 :type :property-type-url
                                 :value ""
                                 :status :ok}
                                {:name "hash"
                                 :type :property-type-hash
                                 :value "hash"
                                 :status :ok}
                                {:name "hash1"
                                 :type :property-type-hash
                                 :value ""
                                 :status :ok}
                                {:name "vec3"
                                 :type :property-type-vector3
                                 :value [1.0 2.0 3.0]
                                 :status :ok}
                                {:name "vec4"
                                 :type :property-type-vector4
                                 :value [4.0 5.0 6.0 7.0]
                                 :status :ok}
                                {:name "quat"
                                 :type :property-type-quat
                                 :value [0.0 72.05474677020722 90.0]
                                 :status :ok}
                                {:name "quat2"
                                 :type :property-type-quat
                                 :value [0.0 0.0 0.0]
                                 :status :ok}
                                {:name "bool"
                                 :type :property-type-boolean
                                 :value false
                                 :status :ok}]
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
            :script-properties []
            :requires []} result))))

(defn- src->properties [src]
  (:script-properties (lua-info src)))

(deftest test-properties
  (is (= [{:name "test"
           :type :property-type-number
           :value 1.1
           :status :ok}]
         (src->properties "go.property(\"test\", 1.1)")))

  (is (= [{:type :property-type-boolean :value true}
          {:type :property-type-boolean :value false}
          {:type :property-type-number :value 1.0}
          {:type :property-type-number :value -1.0}
          {:type :property-type-number :value 0.5}
          {:type :property-type-number :value -0.5}
          {:type :property-type-number :value 2.0e10}
          {:type :property-type-number :value 2.0e-10}
          {:type :property-type-number :value 3.0e10}
          {:type :property-type-number :value 3.0e-10}
          {:type :property-type-number :value -4.0e10}
          {:type :property-type-number :value -4.0e-10}
          {:type :property-type-hash :value ""}
          {:type :property-type-hash :value "aBc3"}
          {:type :property-type-url :value ""}
          {:type :property-type-url :value "foo"}
          {:type :property-type-url :value "socket:/path/to/object#fragment"}
          {:type :property-type-url :value "socket-hash:/path/to/object-hash#fragment-hash"}
          {:type :property-type-vector3 :value [0.0 0.0 0.0]}
          {:type :property-type-vector3 :value [3.0 3.0 3.0]}
          {:type :property-type-vector3 :value [1.0 2.0 3.0]}
          {:type :property-type-vector4 :value [0.0 0.0 0.0 0.0]}
          {:type :property-type-vector4 :value [4.0 4.0 4.0 4.0]}
          {:type :property-type-vector4 :value [1.0 2.0 3.0 4.0]}
          {:type :property-type-quat :value [0.0 0.0 0.0]}
          {:type :property-type-quat :value [0.0 28.072486935852957 90.0]}]
         (map #(select-keys % [:value :type])
              (src->properties
                (string/join "\n" ["go.property(\"test\", true)"
                                   "go.property(\"test\", false)"
                                   "go.property(\"test\", 1)"
                                   "go.property(\"test\", -1)"
                                   "go.property(\"test\", .5)"
                                   "go.property(\"test\", -.5)"
                                   "go.property(\"test\", 2.0E10)"
                                   "go.property(\"test\", 2.0E-10)"
                                   "go.property(\"test\", 3.0e10)"
                                   "go.property(\"test\", 3.0e-10)"
                                   "go.property(\"test\", -4.0e10)"
                                   "go.property(\"test\", -4.0e-10)"
                                   "go.property(\"test\", hash(''))"
                                   "go.property(\"test\", hash('aBc3'))"
                                   "go.property(\"test\", msg.url())"
                                   "go.property(\"test\", msg.url('foo'))"
                                   "go.property(\"test\", msg.url('socket', '/path/to/object', 'fragment'))"
                                   "go.property(\"test\", msg.url(hash('socket-hash'), hash('/path/to/object-hash'), hash('fragment-hash')))"
                                   "go.property(\"test\", vmath.vector3())"
                                   "go.property(\"test\", vmath.vector3(3))"
                                   "go.property(\"test\", vmath.vector3(1, 2, 3))"
                                   "go.property(\"test\", vmath.vector4())"
                                   "go.property(\"test\", vmath.vector4(4))"
                                   "go.property(\"test\", vmath.vector4(1, 2, 3, 4))"
                                   "go.property(\"test\", vmath.quat())"
                                   "go.property(\"test\", vmath.quat(1, 2, 3, 4))"])))))

  (is (= [] (src->properties "foo.property(\"test\", true)")))
  (is (= [] (src->properties "go.property")))
  (is (= [{:status :invalid-args}] (src->properties "go.property()")))
  (is (= [{:status :invalid-args :name "test"}] (src->properties "go.property(\"test\")"))))
