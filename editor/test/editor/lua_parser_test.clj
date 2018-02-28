(ns editor.lua-parser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.lua-parser :refer :all]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support])
  (:import [org.apache.commons.lang RandomStringUtils]))

(deftest test-variables
  (testing "global variable with assignment"
    (is (= #{"foo"} (:vars (lua-info nil "foo=1")))))
  (testing "mulitiple global variable with assignment"
    (is (= #{"foo" "bar"} (:vars (lua-info nil "foo,bar=1,2")))))
  (testing "one local with no assignment"
    (is (= #{"foo"} (:local-vars (lua-info nil "local foo")))))
  (testing "multiple locals with no assignment"
    (is (= #{"foo" "bar"} (:local-vars (lua-info nil "local foo,bar")))))
  (testing "one local with assignment"
    (is (= #{"foo"} (:local-vars (lua-info nil "local foo=1")))))
  (testing "multiple locals with assignment"
    (is (= #{"foo" "bar"} (:local-vars (lua-info nil "local foo,bar = 1,3")))))
  (testing "local with a require assignment"
    (let [code "local mymathmodule = require(\"mymath\")"
          result (select-keys (lua-info nil code) [:local-vars :requires])]
      (is (= {:local-vars #{"mymathmodule"}
              :requires [["mymathmodule" "mymath"]]} result))))
  (testing "global with a require assignment"
    (let [code "mymathmodule = require(\"mymath\")"
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{"mymathmodule"}
              :requires [["mymathmodule" "mymath"]]} result))))
  (testing "multiple local require assignments"
    (let [code "local x = require(\"myx\") \n local y = require(\"myy\")"
          result (select-keys (lua-info nil code) [:local-vars :requires])]
      (is (= {:local-vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "local with multiple require assignments"
    (let [code "local x,y = require(\"myx\"), require(\"myy\")"
          result (select-keys (lua-info nil code) [:local-vars :requires])]
      (is (= {:local-vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "multiple global require assignments"
    (let [code "x = require(\"myx\") \n y = require(\"myy\")"
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result))))
  (testing "multiple require string types"
    (let [code "x = require('myx') \n y = require(\"myy\") \n z = require([==[myz]==])"
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{"x" "y" "z"}
              :requires [["x" "myx"] ["y" "myy"] ["z" "myz"]]} result))))
  (testing "global with multiple require assignments"
    (let [code "x,y = require(\"myx\"), require(\"myy\")"
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{"x" "y"}
              :requires [["x" "myx"] ["y" "myy"]]} result)))))

(deftest test-require
  (testing "bare require function call"
    (let [code "require(\"mymath\")"
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{}
              :requires [[nil "mymath"]]} result))))
  (testing "require call as part of complex expression"
    (let [code (string/join "\n" ["state_rules[hash(\"main\")] = hash_rules("
                                  "    require \"main/state_rules\""
                                  ")"])
          result (select-keys (lua-info nil code) [:vars :requires])]
      (is (= {:vars #{"state_rules"}
              :requires [[nil "main/state_rules"]]} result)))))

(deftest test-functions
  (testing "global function with no params"
    (let [code "function oadd() return num1 end"]
      (is (= {"oadd" {:params []}} (:functions (lua-info nil code))))))
  (testing "local function with no params"
    (let [code "local function oadd() return num1 end"]
      (is (= {"oadd" {:params []}} (:local-functions (lua-info nil code))))))
  (testing "global function with namespace"
    (let [code "function foo.oadd() return num1 end"]
      (is (= {"foo.oadd" {:params []}} (:functions (lua-info nil code))))))
  (testing "global function with params"
    (let [code "function oadd(num1, num2) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}} (:functions (lua-info nil code))))))
  (testing "global multiple functions"
    (let [code "function oadd(num1, num2) return num1 end \n function tadd(foo) return num1 end"]
      (is (= {"oadd" {:params ["num1" "num2"]}, "tadd" {:params ["foo"]}} (:functions (lua-info nil code)))))))

(deftest test-lua-info-with-files
  (testing "variable test file"
    (let [result (lua-info nil (slurp (io/resource "lua/variable_test.lua")))]
      (is (= {:local-vars #{"a" "b" "c" "d"}
              :vars #{"x"}
              :functions {}
              :local-functions {}
              :script-properties []
              :requires []} result))))
  (testing "function test file"
    (let [result (lua-info nil (slurp (io/resource "lua/function_test.lua")))]
      (is (= {:vars #{}
              :local-vars #{}
              :local-functions {"ladd" {:params ["num1" "num2"]}}
              :functions {"add" {:params ["num1" "num2"]}
                          "oadd" {:params ["num1" "num2"]}}
              :script-properties []
              :requires []} result))))
  (testing "require test file"
    (let [result (lua-info nil (slurp (io/resource "lua/require_test.lua")))]
      (is (= {:vars #{"foo"}
              :local-vars #{"bar"}
              :functions {}
              :local-functions {}
              :script-properties []
              :requires [[nil "mymath"] ["foo" "mymath"] ["bar" "mymath"]]} result)))))

(deftest test-lua-info-with-spaceship
  (let [result (lua-info nil (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script")))]
    (is (= {:vars #{"self"}
            :local-vars #{"max_speed" "min_y" "max_y" "p"}
            :functions {"init" {:params ["self"]}
                        "update" {:params ["self" "dt"]}
                        "on_input" {:params ["self" "action_id" "action"]}}
            :local-functions {}
            :script-properties []
            :requires []} result))))

(deftest test-lua-info-with-props-script
  (let [result (lua-info nil (slurp (io/resource "build_project/SideScroller/script/props.script")))]
    (is (= {:vars #{}
            :local-vars #{}
            :functions {}
            :local-functions {}
            :script-properties [{:name "number"
                                 :type :property-type-number
                                 :sub-type 0
                                 :value 100.0
                                 :status :ok}
                                {:name "number1"
                                 :type :property-type-number
                                 :sub-type 0
                                 :value 1.0
                                 :status :ok}
                                {:name "number2"
                                 :type :property-type-number
                                 :sub-type 0
                                 :value 0.1
                                 :status :ok}
                                {:name "number3"
                                 :type :property-type-number
                                 :sub-type 0
                                 :value -0.1
                                 :status :ok}
                                {:name "url"
                                 :type :property-type-url
                                 :sub-type 0
                                 :value "url"
                                 :status :ok}
                                {:name "url1"
                                 :type :property-type-url
                                 :sub-type 0
                                 :value ""
                                 :status :ok}
                                {:name "url2"
                                 :type :property-type-url
                                 :sub-type 0
                                 :value ""
                                 :status :ok}
                                {:name "hash"
                                 :type :property-type-hash
                                 :sub-type 0
                                 :value "hash"
                                 :status :ok}
                                {:name "hash1"
                                 :type :property-type-hash
                                 :sub-type 0
                                 :value ""
                                 :status :ok}
                                {:name "vec3"
                                 :type :property-type-vector3
                                 :sub-type 0
                                 :value [1.0 2.0 3.0]
                                 :status :ok}
                                {:name "vec4"
                                 :type :property-type-vector4
                                 :sub-type 0
                                 :value [4.0 5.0 6.0 7.0]
                                 :status :ok}
                                {:name "quat"
                                 :type :property-type-quat
                                 :sub-type 0
                                 :value [0.0 72.05474677020722 90.0]
                                 :status :ok}
                                {:name "quat2"
                                 :type :property-type-quat
                                 :sub-type 0
                                 :value [0.0 0.0 0.0]
                                 :status :ok}
                                {:name "bool"
                                 :type :property-type-boolean
                                 :sub-type 0
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
    (doall (map #(do (reset! last-fuzz %) (lua-info nil %)) fuzz-codes))

  (let [original (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script"))
        broken-versions (map soil (repeat 100 original))]
    (doall (map #(do (reset! last-broken %) (lua-info nil %)) broken-versions)))))

(deftest string-as-function-parameter-is-ignored
  (let [code "function fun(\"foo\") end"
        result (lua-info nil code)]
    (is (= {:vars #{}
            :local-vars #{}
            :functions {"fun" {:params []}}
            :local-functions {}
            :script-properties []
            :requires []} result))))

(deftest function-following-local-function
  (are [code]
    (= {:vars #{}
        :local-vars #{}
        :functions {}
        :local-functions {}
        :script-properties []
        :requires []}
       (lua-info nil code))

    "local function\nfunction"
    "local function\nfunction foo"
    "local function\nfunction foo("
    "local function\nfunction foo()"
    "local function\nfunction foo(a"
    "local function\nfunction foo(a)"
    "local function\nfunction foo(a,"
    "local function\nfunction foo(a,b"
    "local function\nfunction foo(a,b)"))

(defn- src->properties [workspace src]
  (:script-properties (lua-info workspace src)))

(deftest test-properties
  (test-support/with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          resolve-workspace-resource (partial workspace/resolve-workspace-resource workspace)]
      (is (= [{:name "test"
               :type :property-type-number
               :sub-type 0
               :value 1.1
               :status :ok}]
             (src->properties workspace "go.property(\"test\", 1.1)")))

      (is (= [{:type :property-type-boolean :sub-type 0 :value true}
              {:type :property-type-boolean :sub-type 0 :value false}
              {:type :property-type-number :sub-type 0 :value 1.0}
              {:type :property-type-number :sub-type 0 :value -1.0}
              {:type :property-type-number :sub-type 0 :value 0.5}
              {:type :property-type-number :sub-type 0 :value -0.5}
              {:type :property-type-number :sub-type 0 :value 2.0e10}
              {:type :property-type-number :sub-type 0 :value 2.0e-10}
              {:type :property-type-number :sub-type 0 :value 3.0e10}
              {:type :property-type-number :sub-type 0 :value 3.0e-10}
              {:type :property-type-number :sub-type 0 :value -4.0e10}
              {:type :property-type-number :sub-type 0 :value -4.0e-10}
              {:type :property-type-hash :sub-type 0 :value ""}
              {:type :property-type-hash :sub-type 0 :value "aBc3"}
              {:type :property-type-url :sub-type 0 :value ""}
              {:type :property-type-url :sub-type 0 :value ""}
              {:type :property-type-url :sub-type 0 :value "foo"}
              {:type :property-type-url :sub-type 0 :value "socket:/path/to/object#fragment"}
              {:type :property-type-url :sub-type 0 :value "socket-hash:/path/to/object-hash#fragment-hash"}
              {:type :property-type-vector3 :sub-type 0 :value [0.0 0.0 0.0]}
              {:type :property-type-vector3 :sub-type 0 :value [3.0 3.0 3.0]}
              {:type :property-type-vector3 :sub-type 0 :value [1.0 2.0 3.0]}
              {:type :property-type-vector4 :sub-type 0 :value [0.0 0.0 0.0 0.0]}
              {:type :property-type-vector4 :sub-type 0 :value [4.0 4.0 4.0 4.0]}
              {:type :property-type-vector4 :sub-type 0 :value [1.0 2.0 3.0 4.0]}
              {:type :property-type-quat :sub-type 0 :value [0.0 0.0 0.0]}
              {:type :property-type-quat :sub-type 0 :value [0.0 28.072486935852957 90.0]}
              {:type :property-type-resource :sub-type properties/sub-type-material :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-material :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-material :value (resolve-workspace-resource "/absolute/path/to/resource.material")}
              {:type :property-type-resource :sub-type properties/sub-type-texture :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-texture :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-texture :value (resolve-workspace-resource "/absolute/path/to/resource.atlas")}
              {:type :property-type-resource :sub-type properties/sub-type-textureset :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-textureset :value nil}
              {:type :property-type-resource :sub-type properties/sub-type-textureset :value (resolve-workspace-resource "/absolute/path/to/resource.tilesource")}]
             (map #(select-keys % [:value :type :sub-type])
                  (src->properties workspace
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
                                       "go.property(\"test\", msg.url(''))"
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
                                       "go.property(\"test\", vmath.quat(1, 2, 3, 4))"
                                       "go.property(\"test\", material())"
                                       "go.property(\"test\", material(''))"
                                       "go.property(\"test\", material('/absolute/path/to/resource.material'))"
                                       "go.property(\"test\", texture())"
                                       "go.property(\"test\", texture(''))"
                                       "go.property(\"test\", texture('/absolute/path/to/resource.atlas'))"
                                       "go.property(\"test\", textureset())"
                                       "go.property(\"test\", textureset(''))"
                                       "go.property(\"test\", textureset('/absolute/path/to/resource.tilesource'))"])))))

      (is (= []
             (src->properties workspace "foo.property(\"test\", true)")))
      (is (= []
             (src->properties workspace "go.property")))
      (is (= [{:status :invalid-args}]
             (src->properties workspace "go.property()")))
      (is (= [{:status :invalid-args :name "test"}]
             (src->properties workspace "go.property(\"test\")")))
      (is (= [{:status :invalid-name :name "" :type :property-type-number :sub-type 0 :value 0.0}]
             (src->properties workspace "go.property(\"\", 0.0)")))
      (is (= [{:status :invalid-value :name "test"}]
             (src->properties workspace "go.property(\"test\", \"foo\")"))))))
