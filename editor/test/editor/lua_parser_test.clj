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

(ns editor.lua-parser-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.lua-parser :as lp]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.fn :as fn])
  (:import [org.apache.commons.lang3 RandomStringUtils]))

(defn- lua-info
  ([code]
   (g/with-auto-or-fake-evaluation-context evaluation-context
     (lp/lua-info nil fn/constantly-true code evaluation-context)))
  ([workspace valid-resource-kind? code]
   (g/with-auto-or-fake-evaluation-context evaluation-context
     (lp/lua-info workspace valid-resource-kind? code evaluation-context))))

(deftest test-require
  (testing "bare require function call"
    (let [code "require(\"mymath\")"
          result (:modules (lua-info code))]
      (is (= ["mymath"] result))))
  (testing "require function call with tail function call"
    (let [code "require(\"deep_thought\").get_question(\"42\")"
          result (:modules (lua-info code))]
      (is (= ["deep_thought"] result))))
  (testing "global require function call with tail function call"
    (let [code "_G.require(\"deep_thought\").get_question(\"42\")"
          result (:modules (lua-info code))]
      (is (= ["deep_thought"] result))))
  (testing "require function call with tail function call with local assignment"
    (let [code "local result = require(\"deep_thought\").get_question(\"42\")"
          result (:modules (lua-info code))]
      (is (= ["deep_thought"] result))))
  (testing "global require function call with tail function call with local assignment"
    (let [code "local result = _G.require(\"deep_thought\").get_question(\"42\")"
          result (:modules (lua-info code))]
      (is (= ["deep_thought"] result))))
  (testing "require call as part of complex expression"
    (let [code (string/join "\n" ["state_rules[hash(\"main\")] = hash_rules("
                                  "    require \"main/state_rules\""
                                  ")"])
          result (:modules (lua-info code))]
      (is (= ["main/state_rules"] result)))))

(deftest test-lua-info-with-files
  (testing "require test file"
    (let [result (lua-info (slurp (io/resource "lua/require_test.lua")))]
      (is (= {:modules ["mymath"]
              :script-properties []}
             (select-keys result [:modules :script-properties]))))))

(deftest test-lua-info-with-props-script
  (let [result (lua-info (slurp (io/resource "build_project/SideScroller/script/props.script")))]
    (is (= {:script-properties [{:name "number"
                                 :type :script-property-type-number
                                 :value 100.0
                                 :status :ok}
                                {:name "number1"
                                 :type :script-property-type-number
                                 :value 1.0
                                 :status :ok}
                                {:name "number2"
                                 :type :script-property-type-number
                                 :value 0.1
                                 :status :ok}
                                {:name "number3"
                                 :type :script-property-type-number
                                 :value -0.1
                                 :status :ok}
                                {:name "url"
                                 :type :script-property-type-url
                                 :value "url"
                                 :status :ok}
                                {:name "url1"
                                 :type :script-property-type-url
                                 :value ""
                                 :status :ok}
                                {:name "url2"
                                 :type :script-property-type-url
                                 :value ""
                                 :status :ok}
                                {:name "hash"
                                 :type :script-property-type-hash
                                 :value "hash"
                                 :status :ok}
                                {:name "hash1"
                                 :type :script-property-type-hash
                                 :value ""
                                 :status :ok}
                                {:name "vec3"
                                 :type :script-property-type-vector3
                                 :value [1.0 2.0 3.0]
                                 :status :ok}
                                {:name "vec4"
                                 :type :script-property-type-vector4
                                 :value [4.0 5.0 6.0 7.0]
                                 :status :ok}
                                {:name "quat"
                                 :type :script-property-type-quat
                                 :value [0.0 72.05474677020722 90.0]
                                 :status :ok}
                                {:name "quat2"
                                 :type :script-property-type-quat
                                 :value [0.0 0.0 0.0]
                                 :status :ok}
                                {:name "bool"
                                 :type :script-property-type-boolean
                                 :value false
                                 :status :ok}]
            :modules ["script.test"]}
           (select-keys result [:modules :script-properties])))))

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
    (run! #(do (reset! last-fuzz %) (lua-info %)) fuzz-codes)

    (let [original (slurp (io/resource "build_project/SideScroller/spaceship/spaceship.script"))
          broken-versions (map soil (repeat 100 original))]
      (run! #(do (reset! last-broken %) (lua-info %)) broken-versions))))

(deftest function-following-local-function
  (are [code]
    (= {:script-properties []
        :modules []}
       (select-keys (lua-info code) [:modules :script-properties]))

    "local function\nfunction"
    "local function\nfunction foo"
    "local function\nfunction foo("
    "local function\nfunction foo()"
    "local function\nfunction foo(a"
    "local function\nfunction foo(a)"
    "local function\nfunction foo(a,"
    "local function\nfunction foo(a,b"
    "local function\nfunction foo(a,b)"))

(def ^:private valid-resource-kind?
  (partial contains? #{"atlas" "font" "material" "texture" "tile_source" "render_target"}))

(defn- src->properties [workspace src]
  (:script-properties (lua-info workspace valid-resource-kind? src)))

(deftest test-properties
  (test-support/with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          resolve-workspace-resource (partial workspace/resolve-workspace-resource workspace)]
      (is (= [{:name "test"
               :type :script-property-type-number
               :value 1.1
               :status :ok}]
             (src->properties workspace "go.property(\"test\", 1.1)")))

      (is (= [{:type :script-property-type-boolean :value true}
              {:type :script-property-type-boolean :value false}
              {:type :script-property-type-number :value 1.0}
              {:type :script-property-type-number :value -1.0}
              {:type :script-property-type-number :value 0.5}
              {:type :script-property-type-number :value -0.5}
              {:type :script-property-type-number :value 2.0e10}
              {:type :script-property-type-number :value 2.0e-10}
              {:type :script-property-type-number :value 3.0e10}
              {:type :script-property-type-number :value 3.0e-10}
              {:type :script-property-type-number :value -4.0e10}
              {:type :script-property-type-number :value -4.0e-10}
              {:type :script-property-type-hash :value ""}
              {:type :script-property-type-hash :value "aBc3"}
              {:type :script-property-type-url :value ""}
              {:type :script-property-type-url :value ""}
              {:type :script-property-type-url :value "foo"}
              {:type :script-property-type-url :value "socket:/path/to/object#fragment"}
              {:type :script-property-type-url :value "socket-hash:/path/to/object-hash#fragment-hash"}
              {:type :script-property-type-vector3 :value [0.0 0.0 0.0]}
              {:type :script-property-type-vector3 :value [3.0 3.0 3.0]}
              {:type :script-property-type-vector3 :value [1.0 2.0 3.0]}
              {:type :script-property-type-vector4 :value [0.0 0.0 0.0 0.0]}
              {:type :script-property-type-vector4 :value [4.0 4.0 4.0 4.0]}
              {:type :script-property-type-vector4 :value [1.0 2.0 3.0 4.0]}
              {:type :script-property-type-quat :value [0.0 0.0 0.0]}
              {:type :script-property-type-quat :value [0.0 28.072486935852957 90.0]}
              {:type :script-property-type-resource :resource-kind "atlas" :value nil}
              {:type :script-property-type-resource :resource-kind "atlas" :value nil}
              {:type :script-property-type-resource :resource-kind "atlas" :value (resolve-workspace-resource "/absolute/path/to/resource.atlas")}
              {:type :script-property-type-resource :resource-kind "font" :value nil}
              {:type :script-property-type-resource :resource-kind "font" :value nil}
              {:type :script-property-type-resource :resource-kind "font" :value (resolve-workspace-resource "/absolute/path/to/resource.font")}
              {:type :script-property-type-resource :resource-kind "material" :value nil}
              {:type :script-property-type-resource :resource-kind "material" :value nil}
              {:type :script-property-type-resource :resource-kind "material" :value (resolve-workspace-resource "/absolute/path/to/resource.material")}
              {:type :script-property-type-resource :resource-kind "texture" :value nil}
              {:type :script-property-type-resource :resource-kind "texture" :value nil}
              {:type :script-property-type-resource :resource-kind "texture" :value (resolve-workspace-resource "/absolute/path/to/resource.png")}
              {:type :script-property-type-resource :resource-kind "tile_source" :value nil}
              {:type :script-property-type-resource :resource-kind "tile_source" :value nil}
              {:type :script-property-type-resource :resource-kind "tile_source" :value (resolve-workspace-resource "/absolute/path/to/resource.tilesource")}
              {:type :script-property-type-resource :resource-kind "render_target" :value nil}
              {:type :script-property-type-resource :resource-kind "render_target" :value nil}
              {:type :script-property-type-resource :resource-kind "render_target" :value (resolve-workspace-resource "/absolute/path/to/resource.render_target")}]
             (map #(select-keys % [:value :type :resource-kind])
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
                                       "go.property(\"test\", resource.atlas())"
                                       "go.property(\"test\", resource.atlas(''))"
                                       "go.property(\"test\", resource.atlas('/absolute/path/to/resource.atlas'))"
                                       "go.property(\"test\", resource.font())"
                                       "go.property(\"test\", resource.font(''))"
                                       "go.property(\"test\", resource.font('/absolute/path/to/resource.font'))"
                                       "go.property(\"test\", resource.material())"
                                       "go.property(\"test\", resource.material(''))"
                                       "go.property(\"test\", resource.material('/absolute/path/to/resource.material'))"
                                       "go.property(\"test\", resource.texture())"
                                       "go.property(\"test\", resource.texture(''))"
                                       "go.property(\"test\", resource.texture('/absolute/path/to/resource.png'))"
                                       "go.property(\"test\", resource.tile_source())"
                                       "go.property(\"test\", resource.tile_source(''))"
                                       "go.property(\"test\", resource.tile_source('/absolute/path/to/resource.tilesource'))"
                                       "go.property(\"test\", resource.render_target())"
                                       "go.property(\"test\", resource.render_target(''))"
                                       "go.property(\"test\", resource.render_target('/absolute/path/to/resource.render_target'))"])))))

      (is (= []
             (src->properties workspace "foo.property(\"test\", true)")))
      (is (= []
             (src->properties workspace "go.property")))
      (is (= [{:status :invalid-args}]
             (src->properties workspace "go.property()")))
      (is (= [{:status :invalid-value
               :name "test"}]
             (src->properties workspace "go.property(\"test\")")))
      (is (= [{:status :invalid-args}]
             (src->properties workspace "go.property(\"\", 0.0)")))
      (is (= [{:status :invalid-value
               :name "test"}]
             (src->properties workspace "go.property(\"test\", \"foo\")")))
      (is (= [{:status :invalid-location
               :name "nested"
               :type :script-property-type-boolean
               :value false}]
             (src->properties workspace "function init() go.property('nested', false) end"))))))
