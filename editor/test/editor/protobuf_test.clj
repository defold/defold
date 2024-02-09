;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.protobuf-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.test TestAtlasProto$AtlasAnimation TestDdf$BooleanMsg TestDdf$BytesMsg TestDdf$DefaultValue TestDdf$EmptyMsg TestDdf$JavaCasingMsg TestDdf$Msg TestDdf$NestedMessages TestDdf$NestedMessages$NestedEnum$Enum TestDdf$OptionalNoDefaultValue TestDdf$RepeatedUints TestDdf$SubMsg TestDdf$Transform TestDdf$Uint64Msg]
           [com.dynamo.proto DdfMath$Matrix4 DdfMath$Point3 DdfMath$Quat DdfMath$Vector3 DdfMath$Vector4]
           [com.google.protobuf ByteString]))

(defn- read-text [^java.lang.Class cls s]
  (with-in-str s
    (protobuf/read-text cls (io/reader *in*))))

(defn- round-trip [^java.lang.Class cls m]
  (->> m
    (protobuf/map->str cls)
    (read-text cls)))

(defn- round-trip-data [^java.lang.Class cls s]
  (->> s
    (read-text cls)
    (protobuf/map->str cls)))

(deftest simple
  (let [m {:uint-value 1}
        new-m (round-trip TestDdf$SubMsg m)]
    (is (= 1 (:uint-value new-m)))
    (is (= m new-m))))

(deftest transform
  (let [m {:position [0.0 1.0 2.0]
           :rotation [0.0 1.0 2.0 3.0]}
        new-m (round-trip TestDdf$Transform m)]
    (is (= m new-m))))

(deftest types
  (let [m {:uint-value 1
           :int-value 2
           :string-value "three"
           :vec3-value [0.0 1.0 2.0]
           :vec4-value [0.0 1.0 2.0 3.0]
           :quat-value [0.0 1.0 2.0 3.0]
           :matrix4-value (into [] (map double (range 16)))
           :sub-msg-value {:uint-value 1}
           :enum-value :enum-val0
           :bool-value false}
        new-m (round-trip TestDdf$Msg m)]
    (is (= 1 (:uint-value new-m)))
    (is (= 2 (:int-value new-m)))
    (is (= "three" (:string-value new-m)))
    (is (= false (:bool-value new-m)))
    (is (= m new-m))))

(deftest java-casing
  (let [m {:java-casing "test"}
        new-m (round-trip TestDdf$JavaCasingMsg m)]
    (is (= "test" (:java-casing new-m)))))

(deftest default-vals
  (testing "Defaults readable"
           (let [defaults {:uint-value 10
                           :string-value "test"
                           :quat-value [0.0 0.0 0.0 1.0]
                           :enum-value :enum-val1
                           :bool-value true}
                 new-m (round-trip TestDdf$DefaultValue {})]
             (is (= defaults new-m))
             (doseq [[k v] defaults]
               (is (= v (protobuf/default TestDdf$DefaultValue k)))))))

(deftest optional-no-defaults
  (let [defaults {:uint-value 0
                  :string-value ""
                  :enum-value :enum-val0}
        new-m (round-trip TestDdf$OptionalNoDefaultValue {})]
    (is (= defaults new-m))))

(deftest empty-msg
  (let [m {}
        new-m (round-trip TestDdf$EmptyMsg m)]
    (is (= m new-m))))

(deftest uint64-msg
  (let [m {:uint64-value 1}
        new-m (round-trip TestDdf$Uint64Msg m)]
    (is (= m new-m))))

(deftest repeated-msgs
  (let [m {:id "my_anim"
           :playback :playback-once-forward
           :fps 30
           :flip-horizontal 0
           :flip-vertical 0
           :images [{:image "/path/1.png"}
                    {:image "/path/2.png"}]}
        new-m (round-trip TestAtlasProto$AtlasAnimation m)]
    (is (= m new-m))))

(deftest repeated-uints
  (let [m {:uint-values (into [] (range 10))}
        new-m (round-trip TestDdf$RepeatedUints m)]
    (is (= m new-m))))

(deftest nested-messages
  (let [m {:msg {:enum :enum-val0}
           :multi-msgs (list {:enum :enum-val0})}
        new-m (round-trip TestDdf$NestedMessages m)]
    (is (= m new-m)))
  (let [data "msg {\n  enum: ENUM_VAL0\n}\nmulti_msgs {\n  enum: ENUM_VAL0\n}\n"
        new-data (round-trip-data TestDdf$NestedMessages data)]
    (is (= data new-data))))

(deftest enum-values
  (let [expected (list [:enum-val0 {:display-name "Enum Val0"}]
                       [:enum-val1 {:display-name "Enum Val1"}])
        values (protobuf/enum-values TestDdf$NestedMessages$NestedEnum$Enum)]
    (is (= values expected))))

(deftest boolean-msg
  (let [m {:value false}
        new-m (round-trip TestDdf$BooleanMsg m)]
    (is (= m new-m))))

(deftest bytes-msg
  (let [m {:value (ByteString/copyFromUtf8 "test-string")}
        new-m (round-trip TestDdf$BytesMsg m)]
    (is (= m new-m))))

(deftest field-order
  (is (= :uint-value ((protobuf/fields-by-indices TestDdf$Msg) 1))))

(deftest underscores-to-camel-case-test
  (is (= "Id" (protobuf/underscores-to-camel-case "id")))
  (is (= "StoreFrontImageUrl" (protobuf/underscores-to-camel-case "store_front_image_url")))
  (is (= "IOSExecutableUrl" (protobuf/underscores-to-camel-case "iOSExecutableUrl")))
  (is (= "SomeField_" (protobuf/underscores-to-camel-case "some_field#"))))

(deftest msg->clj-test
  (let [zero (Float/valueOf 0.0)
        one (Float/valueOf 1.0)
        point3 (DdfMath$Point3/getDefaultInstance)
        vector3 (DdfMath$Vector3/getDefaultInstance)
        vector4 (DdfMath$Vector4/getDefaultInstance)
        quat (DdfMath$Quat/getDefaultInstance)
        matrix4 (DdfMath$Matrix4/getDefaultInstance)]

    (testing "Constructs vectors from maps."
      (is (= [(float 1.0) (float 2.0) (float 3.0)]
             (protobuf/msg->clj point3 {:x (float 1.0) :y (float 2.0) :z (float 3.0)})))
      (is (= [(float 1.0) (float 2.0) (float 3.0)]
             (protobuf/msg->clj vector3 {:x (float 1.0) :y (float 2.0) :z (float 3.0)})))
      (is (= [(float 1.0) (float 2.0) (float 3.0) (float 4.0)]
             (protobuf/msg->clj vector4 {:x (float 1.0) :y (float 2.0) :z (float 3.0) :w (float 4.0)})))
      (is (= [(float 1.0) (float 2.0) (float 3.0) (float 4.0)]
             (protobuf/msg->clj quat {:x (float 1.0) :y (float 2.0) :z (float 3.0) :w (float 4.0)})))
      (is (= [(float 0.0) (float 0.1) (float 0.2) (float 0.3)
              (float 1.0) (float 1.1) (float 1.2) (float 1.3)
              (float 2.0) (float 2.1) (float 2.2) (float 2.3)
              (float 3.0) (float 3.1) (float 3.2) (float 3.3)]
             (protobuf/msg->clj matrix4 {:m00 (float 0.0) :m01 (float 0.1) :m02 (float 0.2) :m03 (float 0.3)
                                         :m10 (float 1.0) :m11 (float 1.1) :m12 (float 1.2) :m13 (float 1.3)
                                         :m20 (float 2.0) :m21 (float 2.1) :m22 (float 2.2) :m23 (float 2.3)
                                         :m30 (float 3.0) :m31 (float 3.1) :m32 (float 3.2) :m33 (float 3.3)}))))

    (testing "Common vecmath values share clj instances."
      (is (identical? (protobuf/msg->clj point3 {:x zero :y zero :z zero})
                      (protobuf/msg->clj point3 {:x zero :y zero :z zero})))
      (is (identical? (protobuf/msg->clj vector3 {:x zero :y zero :z zero})
                      (protobuf/msg->clj vector3 {:x zero :y zero :z zero})))
      (is (identical? (protobuf/msg->clj vector3 {:x one :y one :z one})
                      (protobuf/msg->clj vector3 {:x one :y one :z one})))
      (is (identical? (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})
                      (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})))
      (is (identical? (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})
                      (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})))
      (is (identical? (protobuf/msg->clj vector4 {:x one :y one :z one :w one})
                      (protobuf/msg->clj vector4 {:x one :y one :z one :w one})))
      (is (identical? (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})
                      (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})))
      (is (identical? (protobuf/msg->clj quat {:x zero :y zero :z zero :w one})
                      (protobuf/msg->clj quat {:x zero :y zero :z zero :w one})))
      (is (identical? (protobuf/msg->clj matrix4 {:m00 one :m01 zero :m02 zero :m03 zero
                                                  :m10 zero :m11 one :m12 zero :m13 zero
                                                  :m20 zero :m21 zero :m22 one :m23 zero
                                                  :m30 zero :m31 zero :m32 zero :m33 one})
                      (protobuf/msg->clj matrix4 {:m00 one :m01 zero :m02 zero :m03 zero
                                                  :m10 zero :m11 one :m12 zero :m13 zero
                                                  :m20 zero :m21 zero :m22 one :m23 zero
                                                  :m30 zero :m31 zero :m32 zero :m33 one}))))

    (testing "Shared instances have correct values."
      (is (= [zero zero zero]
             (protobuf/msg->clj point3 {:x zero :y zero :z zero})))
      (is (= [zero zero zero]
             (protobuf/msg->clj vector3 {:x zero :y zero :z zero})))
      (is (= [one one one]
             (protobuf/msg->clj vector3 {:x one :y one :z one})))
      (is (= [zero zero zero zero]
             (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})))
      (is (= [zero zero zero one]
             (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})))
      (is (= [one one one one]
             (protobuf/msg->clj vector4 {:x one :y one :z one :w one})))
      (is (= [one one one zero]
             (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})))
      (is (= [zero zero zero one]
             (protobuf/msg->clj quat {:x zero :y zero :z zero :w one})))
      (is (= [one zero zero zero
              zero one zero zero
              zero zero one zero
              zero zero zero one]
             (protobuf/msg->clj matrix4 {:m00 one :m01 zero :m02 zero :m03 zero
                                         :m10 zero :m11 one :m12 zero :m13 zero
                                         :m20 zero :m21 zero :m22 one :m23 zero
                                         :m30 zero :m31 zero :m32 zero :m33 one}))))))
