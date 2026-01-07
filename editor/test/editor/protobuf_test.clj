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

(ns editor.protobuf-test
  (:require [clojure.test :refer :all]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.test TestDdf$BooleanMsg TestDdf$BytesMsg TestDdf$DefaultValue TestDdf$EmptyMsg TestDdf$JavaCasingMsg TestDdf$Msg TestDdf$NestedDefaults TestDdf$NestedDefaultsSubMsg TestDdf$NestedMessages TestDdf$NestedMessages$NestedEnum$Enum TestDdf$NestedRequireds TestDdf$NestedRequiredsSubMsg TestDdf$OptionalNoDefaultValue TestDdf$RepeatedUints TestDdf$ResourceDefaulted TestDdf$ResourceDefaultedNested TestDdf$ResourceDefaultedRepeatedlyNested TestDdf$ResourceFields TestDdf$ResourceRepeated TestDdf$ResourceRepeatedNested TestDdf$ResourceRepeatedRepeatedlyNested TestDdf$ResourceSimple TestDdf$ResourceSimpleNested TestDdf$ResourceSimpleRepeatedlyNested TestDdf$SubMsg TestDdf$Transform TestDdf$Uint64Msg]
           [com.dynamo.proto DdfMath$Matrix4 DdfMath$Point3 DdfMath$Quat DdfMath$Vector3 DdfMath$Vector3One DdfMath$Vector4 DdfMath$Vector4One DdfMath$Vector4WOne]
           [com.google.protobuf ByteString]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

(defn- round-trip [^Class cls pb-map]
  (->> pb-map
       (protobuf/map->str cls)
       (protobuf/str->map-with-defaults cls)))

(defn- round-trip-data [^Class cls pb-str]
  (->> pb-str
       (protobuf/str->map-with-defaults cls)
       (protobuf/map->str cls)))

(deftest boolean->int
  (is (= 0 (protobuf/boolean->int nil)))
  (is (= 0 (protobuf/boolean->int false)))
  (is (= 1 (protobuf/boolean->int true)))
  (is (= 0 (protobuf/boolean->int (Boolean. false))))
  (is (= 1 (protobuf/boolean->int (Boolean. true)))))

(deftest int->boolean
  (is (false? (protobuf/int->boolean nil)))
  (is (false? (protobuf/int->boolean 0)))
  (is (true? (protobuf/int->boolean 1)))
  (is (true? (protobuf/int->boolean 2)))
  (is (true? (protobuf/int->boolean -1)))
  (is (false? (protobuf/int->boolean (Integer. 0))))
  (is (true? (protobuf/int->boolean (Integer. 1)))))

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
  (let [m {:simples [{:image "/path/1.png"}
                     {:image "/path/2.png"}]}
        new-m (round-trip TestDdf$ResourceSimpleRepeatedlyNested m)]
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
  (let [zero (Float/valueOf (float 0.0))
        one (Float/valueOf (float 1.0))
        point3 (DdfMath$Point3/getDefaultInstance)
        vector3 (DdfMath$Vector3/getDefaultInstance)
        vector3-one (DdfMath$Vector3One/getDefaultInstance)
        vector4 (DdfMath$Vector4/getDefaultInstance)
        vector4-one (DdfMath$Vector4One/getDefaultInstance)
        vector4-w-one (DdfMath$Vector4WOne/getDefaultInstance)
        quat (DdfMath$Quat/getDefaultInstance)
        matrix4 (DdfMath$Matrix4/getDefaultInstance)]

    (testing "Constructs vectors from maps."
      (is (= [(float 1.0) (float 2.0) (float 3.0)]
             (protobuf/msg->clj point3 {:x (float 1.0) :y (float 2.0) :z (float 3.0)})))
      (is (= [(float 1.0) (float 2.0) (float 3.0)]
             (protobuf/msg->clj vector3 {:x (float 1.0) :y (float 2.0) :z (float 3.0)})))
      (is (= [(float 2.0) (float 3.0) (float 4.0)]
             (protobuf/msg->clj vector3-one {:x (float 2.0) :y (float 3.0) :z (float 4.0)})))
      (is (= [(float 1.0) (float 2.0) (float 3.0) (float 4.0)]
             (protobuf/msg->clj vector4 {:x (float 1.0) :y (float 2.0) :z (float 3.0) :w (float 4.0)})))
      (is (= [(float 2.0) (float 3.0) (float 4.0) (float 5.0)]
             (protobuf/msg->clj vector4-one {:x (float 2.0) :y (float 3.0) :z (float 4.0) :w (float 5.0)})))
      (is (= [(float 2.0) (float 3.0) (float 4.0) (float 5.0)]
             (protobuf/msg->clj vector4-w-one {:x (float 2.0) :y (float 3.0) :z (float 4.0) :w (float 5.0)})))
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
      (is (identical? (protobuf/msg->clj vector3-one {:x zero :y zero :z zero})
                      (protobuf/msg->clj vector3-one {:x zero :y zero :z zero})))
      (is (identical? (protobuf/msg->clj vector3-one {:x one :y one :z one})
                      (protobuf/msg->clj vector3-one {:x one :y one :z one})))
      (is (identical? (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})
                      (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})))
      (is (identical? (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})
                      (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})))
      (is (identical? (protobuf/msg->clj vector4 {:x one :y one :z one :w one})
                      (protobuf/msg->clj vector4 {:x one :y one :z one :w one})))
      (is (identical? (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})
                      (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})))
      (is (identical? (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w zero})
                      (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w zero})))
      (is (identical? (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w one})
                      (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w one})))
      (is (identical? (protobuf/msg->clj vector4-one {:x one :y one :z one :w one})
                      (protobuf/msg->clj vector4-one {:x one :y one :z one :w one})))
      (is (identical? (protobuf/msg->clj vector4-one {:x one :y one :z one :w zero})
                      (protobuf/msg->clj vector4-one {:x one :y one :z one :w zero})))
      (is (identical? (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w zero})
                      (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w zero})))
      (is (identical? (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w one})
                      (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w one})))
      (is (identical? (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w one})
                      (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w one})))
      (is (identical? (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w zero})
                      (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w zero})))
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
      (is (= [zero zero zero]
             (protobuf/msg->clj vector3-one {:x zero :y zero :z zero})))
      (is (= [one one one]
             (protobuf/msg->clj vector3-one {:x one :y one :z one})))
      (is (= [zero zero zero zero]
             (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w zero})))
      (is (= [zero zero zero one]
             (protobuf/msg->clj vector4 {:x zero :y zero :z zero :w one})))
      (is (= [one one one one]
             (protobuf/msg->clj vector4 {:x one :y one :z one :w one})))
      (is (= [one one one zero]
             (protobuf/msg->clj vector4 {:x one :y one :z one :w zero})))
      (is (= [zero zero zero zero]
             (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w zero})))
      (is (= [zero zero zero one]
             (protobuf/msg->clj vector4-one {:x zero :y zero :z zero :w one})))
      (is (= [one one one one]
             (protobuf/msg->clj vector4-one {:x one :y one :z one :w one})))
      (is (= [one one one zero]
             (protobuf/msg->clj vector4-one {:x one :y one :z one :w zero})))
      (is (= [zero zero zero zero]
             (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w zero})))
      (is (= [zero zero zero one]
             (protobuf/msg->clj vector4-w-one {:x zero :y zero :z zero :w one})))
      (is (= [one one one one]
             (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w one})))
      (is (= [one one one zero]
             (protobuf/msg->clj vector4-w-one {:x one :y one :z one :w zero})))
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

(deftest resource-field-path-specs-test
  (is (= [[:image]] (protobuf/resource-field-path-specs TestDdf$ResourceSimple)))
  (is (= [[{:image "/default.png"}]] (protobuf/resource-field-path-specs TestDdf$ResourceDefaulted)))
  (is (= [[[:images]]] (protobuf/resource-field-path-specs TestDdf$ResourceRepeated)))
  (is (= [[:simple :image]] (protobuf/resource-field-path-specs TestDdf$ResourceSimpleNested)))
  (is (= [[:defaulted {:image "/default.png"}]] (protobuf/resource-field-path-specs TestDdf$ResourceDefaultedNested)))
  (is (= [[:repeated [:images]]] (protobuf/resource-field-path-specs TestDdf$ResourceRepeatedNested)))
  (is (= [[[:simples] :image]] (protobuf/resource-field-path-specs TestDdf$ResourceSimpleRepeatedlyNested)))
  (is (= [[[:defaulteds] {:image "/default.png"}]] (protobuf/resource-field-path-specs TestDdf$ResourceDefaultedRepeatedlyNested)))
  (is (= [[[:repeateds] [:images]]] (protobuf/resource-field-path-specs TestDdf$ResourceRepeatedRepeatedlyNested))))

(deftest get-field-value-paths-fn-test
  (testing "Simple"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[:image]])
            {})))
    (is (= [[[:image] nil]]
           ((protobuf/get-field-value-paths-fn [[:image]])
            {:image nil})))
    (is (= [[[:image] "/image.png"]]
           ((protobuf/get-field-value-paths-fn [[:image]])
            {:image "/image.png"}))))

  (testing "Defaulted"
    (is (= [[[:image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[{:image "/default.png"}]])
            {})))
    (is (= [[[:image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[{:image "/default.png"}]])
            {:image nil})))
    (is (= [[[:image] "/image.png"]]
           ((protobuf/get-field-value-paths-fn [[{:image "/default.png"}]])
            {:image "/image.png"}))))

  (testing "Repeated"
    (is (= [[[:images 0] nil]
            [[:images 1] nil]]
           ((protobuf/get-field-value-paths-fn [[[:images]]])
            {:images [nil
                      nil]})))
    (is (= [[[:images 0] "/image0.png"]
            [[:images 1] "/image1.png"]]
           ((protobuf/get-field-value-paths-fn [[[:images]]])
            {:images ["/image0.png"
                      "/image1.png"]}))))

  (testing "Simple nested"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[:simple :image]])
            {:simple {}})))
    (is (= [[[:simple :image] nil]]
           ((protobuf/get-field-value-paths-fn [[:simple :image]])
            {:simple {:image nil}})))
    (is (= [[[:simple :image] "/image.png"]]
           ((protobuf/get-field-value-paths-fn [[:simple :image]])
            {:simple {:image "/image.png"}}))))

  (testing "Defaulted nested"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[:defaulted {:image "/default.png"}]])
            {})))
    (is (= [[[:defaulted :image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[:defaulted {:image "/default.png"}]])
            {:defaulted {}})))
    (is (= [[[:defaulted :image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[:defaulted {:image "/default.png"}]])
            {:defaulted {:image nil}})))
    (is (= [[[:defaulted :image] "/image.png"]]
           ((protobuf/get-field-value-paths-fn [[:defaulted {:image "/default.png"}]])
            {:defaulted {:image "/image.png"}}))))

  (testing "Repeated nested"
    (is (= [[[:repeated :images 0] nil]
            [[:repeated :images 1] nil]]
           ((protobuf/get-field-value-paths-fn [[:repeated [:images]]])
            {:repeated {:images [nil
                                 nil]}})))
    (is (= [[[:repeated :images 0] "/image0.png"]
            [[:repeated :images 1] "/image1.png"]]
           ((protobuf/get-field-value-paths-fn [[:repeated [:images]]])
            {:repeated {:images ["/image0.png"
                                 "/image1.png"]}}))))

  (testing "Simple repeatedly nested"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:simples] :image]])
            {:simples [{}
                       {}]})))
    (is (= [[[:simples 0 :image] nil]
            [[:simples 1 :image] nil]]
           ((protobuf/get-field-value-paths-fn [[[:simples] :image]])
            {:simples [{:image nil}
                       {:image nil}]})))
    (is (= [[[:simples 0 :image] "/image0.png"]
            [[:simples 1 :image] "/image1.png"]]
           ((protobuf/get-field-value-paths-fn [[[:simples] :image]])
            {:simples [{:image "/image0.png"}
                       {:image "/image1.png"}]}))))

  (testing "Defaulted repeatedly nested"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:defaulteds] {:image "/default.png"}]])
            {:defaulteds []})))
    (is (= [[[:defaulteds 0 :image] "/default.png"]
            [[:defaulteds 1 :image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[[:defaulteds] {:image "/default.png"}]])
            {:defaulteds [{}
                          {}]})))
    (is (= [[[:defaulteds 0 :image] "/default.png"]
            [[:defaulteds 1 :image] "/default.png"]]
           ((protobuf/get-field-value-paths-fn [[[:defaulteds] {:image "/default.png"}]])
            {:defaulteds [{:image nil}
                          {:image nil}]})))
    (is (= [[[:defaulteds 0 :image] "/image0.png"]
            [[:defaulteds 1 :image] "/image1.png"]]
           ((protobuf/get-field-value-paths-fn [[[:defaulteds] {:image "/default.png"}]])
            {:defaulteds [{:image "/image0.png"}
                          {:image "/image1.png"}]}))))

  (testing "Repeated repeatedly nested"
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {})))
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {:repeateds []})))
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {:repeateds [{}
                         {}]})))
    (is (= []
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {:repeateds [{:images []}
                         {:images []}]})))
    (is (= [[[:repeateds 0 :images 0] nil]
            [[:repeateds 0 :images 1] nil]
            [[:repeateds 1 :images 0] nil]
            [[:repeateds 1 :images 1] nil]]
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {:repeateds [{:images [nil
                                   nil]}
                         {:images [nil
                                   nil]}]})))
    (is (= [[[:repeateds 0 :images 0] "/image00.png"]
            [[:repeateds 0 :images 1] "/image01.png"]
            [[:repeateds 1 :images 0] "/image10.png"]
            [[:repeateds 1 :images 1] "/image11.png"]]
           ((protobuf/get-field-value-paths-fn [[[:repeateds] [:images]]])
            {:repeateds [{:images ["/image00.png"
                                   "/image01.png"]}
                         {:images ["/image10.png"
                                   "/image11.png"]}]})))))

;; -----------------------------------------------------------------------------
;; make-map-with-defaults
;; -----------------------------------------------------------------------------

(deftest make-map-with-defaults-unspecified-test
  (is (= {:optional-with-default "default"
          :optional-without-default ""
          :optional-message {:optional-string "default"
                             :optional-int 10
                             :optional-quat [0.0 0.0 0.0 1.0]
                             :optional-enum :enum-val1
                             :optional-bool true}}
         (protobuf/make-map-with-defaults TestDdf$NestedDefaults)))
  (is (= {:optional-message {}} ; TODO(save-value): Should this be here? It contains required fields.
         (protobuf/make-map-with-defaults TestDdf$NestedRequireds)))
  (is (= {:optional-resource "/default"}
         (protobuf/make-map-with-defaults TestDdf$ResourceFields))))

(deftest make-map-with-defaults-specified-overrides-defaults-test
  (is (= {:required-string "overridden required_string"
          :optional-with-default "overridden with_default"
          :optional-without-default "overridden without_default"
          :optional-message {:optional-string "overridden optional_string"
                             :optional-int 11
                             :optional-quat [1.0 2.0 3.0 4.0]
                             :optional-enum :enum-val0
                             :optional-bool false}
          :repeated-message [{:optional-string "default"
                              :optional-int 10
                              :optional-quat [0.0 0.0 0.0 1.0]
                              :optional-enum :enum-val1
                              :optional-bool true}
                             {:optional-string "overridden optional_string"
                              :optional-int 11
                              :optional-quat [1.0 2.0 3.0 4.0]
                              :optional-enum :enum-val0
                              :optional-bool false}]
          :repeated-int [0
                         1]}
         (protobuf/make-map-with-defaults TestDdf$NestedDefaults
           :required-string "overridden required_string"
           :optional-with-default "overridden with_default"
           :optional-without-default "overridden without_default"
           :optional-message (protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg
                               :optional-string "overridden optional_string"
                               :optional-int 11
                               :optional-quat [1.0 2.0 3.0 4.0]
                               :optional-enum :enum-val0
                               :optional-bool false)
           :repeated-message [(protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg)
                              (protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg
                                :optional-string "overridden optional_string"
                                :optional-int 11
                                :optional-quat [1.0 2.0 3.0 4.0]
                                :optional-enum :enum-val0
                                :optional-bool false)]
           :repeated-int [0
                          1])))
  (is (= {:required-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :optional-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :repeated-message [{}
                             {:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}]}
         (protobuf/make-map-with-defaults TestDdf$NestedRequireds
           :required-message (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string "overridden required_string"
                               :required-quat [1.0 2.0 3.0 4.0]
                               :required-enum :enum-val1)
           :optional-message (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string "overridden required_string"
                               :required-quat [1.0 2.0 3.0 4.0]
                               :required-enum :enum-val1)
           :repeated-message [(protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg)
                              (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                                :required-string "overridden required_string"
                                :required-quat [1.0 2.0 3.0 4.0]
                                :required-enum :enum-val1)])))
  (is (= {:optional-resource "/overridden optional_resource"}
         (protobuf/make-map-with-defaults TestDdf$ResourceFields
           :optional-resource "/overridden optional_resource"))))

(deftest make-map-with-defaults-specified-equals-defaults-test
  (is (= {:required-string ""
          :optional-with-default "overridden with_default"
          :optional-without-default "overridden without_default"
          :optional-message {:optional-string "overridden optional_string"
                             :optional-int 11
                             :optional-quat [1.0 2.0 3.0 4.0]
                             :optional-enum :enum-val0
                             :optional-bool false}
          :repeated-message [{:optional-string "default"
                              :optional-int 10
                              :optional-quat [0.0 0.0 0.0 1.0]
                              :optional-enum :enum-val1
                              :optional-bool true}
                             {:optional-string "overridden optional_string"
                              :optional-int 11
                              :optional-quat [1.0 2.0 3.0 4.0]
                              :optional-enum :enum-val0
                              :optional-bool false}]}
         (protobuf/make-map-with-defaults TestDdf$NestedDefaults
           :required-string ""
           :optional-with-default "overridden with_default"
           :optional-without-default "overridden without_default"
           :optional-message (protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg
                               :optional-string "overridden optional_string"
                               :optional-int 11
                               :optional-quat [1.0 2.0 3.0 4.0]
                               :optional-enum :enum-val0
                               :optional-bool false)
           :repeated-message [(protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg)
                              (protobuf/make-map-with-defaults TestDdf$NestedDefaultsSubMsg
                                :optional-string "overridden optional_string"
                                :optional-int 11
                                :optional-quat [1.0 2.0 3.0 4.0]
                                :optional-enum :enum-val0
                                :optional-bool false)])))
  (is (= {:required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :optional-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :repeated-message [{}
                             {:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}]}
         (protobuf/make-map-with-defaults TestDdf$NestedRequireds
           :required-message (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string ""
                               :required-quat [0.0 0.0 0.0 1.0]
                               :required-enum :enum-val0)
           :optional-message (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string ""
                               :required-quat [0.0 0.0 0.0 1.0]
                               :required-enum :enum-val0)
           :repeated-message [(protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg)
                              (protobuf/make-map-with-defaults TestDdf$NestedRequiredsSubMsg
                                :required-string ""
                                :required-quat [0.0 0.0 0.0 1.0]
                                :required-enum :enum-val0)])))
  (is (= {:optional-resource "/default"}
         (protobuf/make-map-with-defaults TestDdf$ResourceFields
           :optional-resource "/default"))))

(deftest make-map-with-defaults-resources
  (assert (= "" (protobuf/default TestDdf$ResourceSimple :image)))
  (assert (= "/default" (protobuf/default TestDdf$ResourceFields :optional-resource)))
  (is (= {:image ""}
         (protobuf/make-map-with-defaults TestDdf$ResourceSimple
           :image ""))
      "Keep empty paths if equal to the default.")
  (is (= {:optional-resource ""}
         (protobuf/make-map-with-defaults TestDdf$ResourceFields
           :optional-resource ""))
      "Keep empty paths if not equal to the default.")
  (is (= {:optional-resource "/default"}
         (protobuf/make-map-with-defaults TestDdf$ResourceFields
           :optional-resource "/default"))
      "Keep non-empty paths even if equal to the default."))

;; -----------------------------------------------------------------------------
;; read-map-with-defaults
;; -----------------------------------------------------------------------------

(defn- read-map-with-defaults [^Class cls ^String pb-str]
  (with-open [reader (StringReader. pb-str)]
    (protobuf/read-map-with-defaults cls reader)))

(deftest read-map-with-defaults-unspecified-test
  (is (= {:required-string ""
          :optional-with-default "default"
          :optional-without-default ""
          :optional-message {:optional-string "default"
                             :optional-int 10
                             :optional-quat [0.0 0.0 0.0 1.0]
                             :optional-enum :enum-val1
                             :optional-bool true}}
         (read-map-with-defaults TestDdf$NestedDefaults "required_string: ''")))
  (is (= {:optional-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}}
         (read-map-with-defaults TestDdf$NestedRequireds "
required_message {
  required_string: ''
  required_quat {}
  required_enum: ENUM_VAL0
}")))
  (is (= {:optional-resource "/default"}
         (read-map-with-defaults TestDdf$ResourceFields ""))))

(deftest read-map-with-defaults-specified-overrides-defaults-test
  (is (= {:required-string "overridden required_string"
          :optional-with-default "overridden with_default"
          :optional-without-default "overridden without_default"
          :optional-message {:optional-string "overridden optional_string"
                             :optional-int 11
                             :optional-quat [1.0 2.0 3.0 4.0]
                             :optional-enum :enum-val0
                             :optional-bool false}
          :repeated-message [{:optional-string "default"
                              :optional-int 10
                              :optional-quat [0.0 0.0 0.0 1.0]
                              :optional-enum :enum-val1
                              :optional-bool true}
                             {:optional-string "overridden optional_string"
                              :optional-int 11
                              :optional-quat [1.0 2.0 3.0 4.0]
                              :optional-enum :enum-val0
                              :optional-bool false}]
          :repeated-int [0
                         1]}
         (read-map-with-defaults TestDdf$NestedDefaults "
required_string: 'overridden required_string'
optional_with_default: 'overridden with_default'
optional_without_default: 'overridden without_default'
optional_message {
  optional_string: 'overridden optional_string'
  optional_int: 11
  optional_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  optional_enum: ENUM_VAL0
  optional_bool: false
}
repeated_message {
}
repeated_message {
  optional_string: 'overridden optional_string'
  optional_int: 11
  optional_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  optional_enum: ENUM_VAL0
  optional_bool: false
}
repeated_int: 0
repeated_int: 1")))
  (is (= {:optional-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :required-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :repeated-message [{:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}
                             {:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}]}
         (read-map-with-defaults TestDdf$NestedRequireds "
required_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
optional_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
repeated_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
repeated_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}")))
  (is (= {:optional-resource "/overridden optional_resource"}
         (read-map-with-defaults TestDdf$ResourceFields "optional_resource: '/overridden optional_resource'"))))

(deftest read-map-with-defaults-specified-equals-defaults-test
  (is (= {:required-string ""
          :optional-with-default "default"
          :optional-without-default ""
          :optional-message {:optional-string "default"
                             :optional-int 10
                             :optional-quat [0.0 0.0 0.0 1.0]
                             :optional-enum :enum-val1
                             :optional-bool true}
          :repeated-message [{:optional-string "default"
                              :optional-int 10
                              :optional-quat [0.0 0.0 0.0 1.0]
                              :optional-enum :enum-val1
                              :optional-bool true}
                             {:optional-string "default"
                              :optional-int 10
                              :optional-quat [0.0 0.0 0.0 1.0]
                              :optional-enum :enum-val1
                              :optional-bool true}]
          :repeated-int [0
                         0]}
         (read-map-with-defaults TestDdf$NestedDefaults "
required_string: ''
optional_with_default: 'default'
optional_without_default: ''
optional_message {
  optional_string: 'default'
  optional_int: 10
  optional_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  optional_enum: ENUM_VAL1
  optional_bool: true
}
repeated_message {
}
repeated_message {
  optional_string: 'default'
  optional_int: 10
  optional_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  optional_enum: ENUM_VAL1
  optional_bool: true
}
repeated_int: 0
repeated_int: 0")))
  (is (= {:optional-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :repeated-message [{:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}
                             {:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}]}
         (read-map-with-defaults TestDdf$NestedRequireds "
required_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
optional_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
repeated_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
repeated_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}")))
  (is (= {:optional-resource "/default"}
         (read-map-with-defaults TestDdf$ResourceFields "optional_resource: '/default'"))))

(deftest read-map-with-defaults-resources
  (assert (= "" (protobuf/default TestDdf$ResourceSimple :image)))
  (assert (= "/default" (protobuf/default TestDdf$ResourceFields :optional-resource)))
  (is (= {:image ""}
         (read-map-with-defaults TestDdf$ResourceSimple "image: ''"))
      "Keep empty paths if equal to the default.")
  (is (= {:optional-resource ""}
         (read-map-with-defaults TestDdf$ResourceFields "optional_resource: ''"))
      "Keep empty paths if not equal to the default.")
  (is (= {:optional-resource "/default"}
         (read-map-with-defaults TestDdf$ResourceFields "optional_resource: '/default'"))
      "Keep non-empty paths even if equal to the default."))

;; -----------------------------------------------------------------------------
;; make-map-without-defaults
;; -----------------------------------------------------------------------------

(deftest make-map-without-defaults-unspecified-test
  (is (= {}
         (protobuf/make-map-without-defaults TestDdf$NestedDefaults)))
  (is (= {}
         (protobuf/make-map-without-defaults TestDdf$NestedRequireds)))
  (is (= {}
         (protobuf/make-map-without-defaults TestDdf$ResourceFields))))

(deftest make-map-without-defaults-specified-overrides-defaults-test
  (is (= {:required-string "overridden required_string"
          :optional-with-default "overridden with_default"
          :optional-without-default "overridden without_default"
          :optional-message {:optional-string "overridden optional_string"
                             :optional-int 11
                             :optional-quat [1.0 2.0 3.0 4.0]
                             :optional-enum :enum-val0
                             :optional-bool false}
          :repeated-message [{}
                             {:optional-string "overridden optional_string"
                              :optional-int 11
                              :optional-quat [1.0 2.0 3.0 4.0]
                              :optional-enum :enum-val0
                              :optional-bool false}]
          :repeated-int [0
                         1]}
         (protobuf/make-map-without-defaults TestDdf$NestedDefaults
           :required-string "overridden required_string"
           :optional-with-default "overridden with_default"
           :optional-without-default "overridden without_default"
           :optional-message (protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg
                               :optional-string "overridden optional_string"
                               :optional-int 11
                               :optional-quat [1.0 2.0 3.0 4.0]
                               :optional-enum :enum-val0
                               :optional-bool false)
           :repeated-message [(protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg)
                              (protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg
                                :optional-string "overridden optional_string"
                                :optional-int 11
                                :optional-quat [1.0 2.0 3.0 4.0]
                                :optional-enum :enum-val0
                                :optional-bool false)]
           :repeated-int [0
                          1])))
  (is (= {:optional-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :required-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :repeated-message [{}
                             {:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}]}
         (protobuf/make-map-without-defaults TestDdf$NestedRequireds
           :optional-message (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string "overridden required_string"
                               :required-quat [1.0 2.0 3.0 4.0]
                               :required-enum :enum-val1)
           :required-message (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string "overridden required_string"
                               :required-quat [1.0 2.0 3.0 4.0]
                               :required-enum :enum-val1)
           :repeated-message [(protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg)
                              (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                                :required-string "overridden required_string"
                                :required-quat [1.0 2.0 3.0 4.0]
                                :required-enum :enum-val1)])))
  (is (= {:optional-resource "/overridden optional_resource"}
         (protobuf/make-map-without-defaults TestDdf$ResourceFields
           :optional-resource "/overridden optional_resource"))))

(deftest make-map-without-defaults-specified-equals-defaults-test
  (is (= {:required-string ""
          :repeated-message [{}
                             {}]
          :repeated-int [0
                         0]}
         (protobuf/make-map-without-defaults TestDdf$NestedDefaults
           :required-string ""
           :optional-with-default "default"
           :optional-without-default ""
           :optional-message (protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg
                               :optional-string "default"
                               :optional-int 10
                               :optional-quat [0.0 0.0 0.0 1.0]
                               :optional-enum :enum-val1
                               :optional-bool true)
           :repeated-message [(protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg)
                              (protobuf/make-map-without-defaults TestDdf$NestedDefaultsSubMsg
                                :optional-string "default"
                                :optional-int 10
                                :optional-quat [0.0 0.0 0.0 1.0]
                                :optional-enum :enum-val1
                                :optional-bool true)]
           :repeated-int [0
                          0])))
  (is (= {:optional-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :repeated-message [{}
                             {:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}]}
         (protobuf/make-map-without-defaults TestDdf$NestedRequireds
           :optional-message (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string ""
                               :required-quat [0.0 0.0 0.0 1.0]
                               :required-enum :enum-val0)
           :required-message (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                               :required-string ""
                               :required-quat [0.0 0.0 0.0 1.0]
                               :required-enum :enum-val0)
           :repeated-message [(protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg)
                              (protobuf/make-map-without-defaults TestDdf$NestedRequiredsSubMsg
                                :required-string ""
                                :required-quat [0.0 0.0 0.0 1.0]
                                :required-enum :enum-val0)])))
  (is (= {:optional-resource "/default"}
         (protobuf/make-map-without-defaults TestDdf$ResourceFields
           :optional-resource "/default"))))

(deftest make-map-without-defaults-resources
  (assert (= "" (protobuf/default TestDdf$ResourceSimple :image)))
  (assert (= "/default" (protobuf/default TestDdf$ResourceFields :optional-resource)))
  (is (= {}
         (protobuf/make-map-without-defaults TestDdf$ResourceSimple
           :image ""))
      "Remove empty paths if equal to the default.")
  (is (= {:optional-resource ""}
         (protobuf/make-map-without-defaults TestDdf$ResourceFields
           :optional-resource ""))
      "Keep empty paths if not equal to the default.")
  (is (= {:optional-resource "/default"}
         (protobuf/make-map-without-defaults TestDdf$ResourceFields
           :optional-resource "/default"))
      "Keep non-empty paths even if equal to the default."))

;; -----------------------------------------------------------------------------
;; read-map-without-defaults
;; -----------------------------------------------------------------------------

(defn- read-map-without-defaults [^Class cls ^String pb-str]
  (with-open [reader (StringReader. pb-str)]
    (protobuf/read-map-without-defaults cls reader)))

(deftest read-map-without-defaults-unspecified-test
  (is (= {:required-string ""}
         (read-map-without-defaults TestDdf$NestedDefaults "required_string: ''")))
  (is (= {:required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}}
         (read-map-without-defaults TestDdf$NestedRequireds "
required_message {
  required_string: ''
  required_quat {}
  required_enum: ENUM_VAL0
}")))
  (is (= {}
         (read-map-without-defaults TestDdf$ResourceFields ""))))

(deftest read-map-without-defaults-specified-overrides-defaults-test
  (is (= {:required-string "overridden required_string"
          :optional-with-default "overridden with_default"
          :optional-without-default "overridden without_default"
          :optional-message {:optional-string "overridden optional_string"
                             :optional-int 11
                             :optional-quat [1.0 2.0 3.0 4.0]
                             :optional-enum :enum-val0
                             :optional-bool false}
          :repeated-message [{}
                             {:optional-string "overridden optional_string"
                              :optional-int 11
                              :optional-quat [1.0 2.0 3.0 4.0]
                              :optional-enum :enum-val0
                              :optional-bool false}]
          :repeated-int [0
                         1]}
         (read-map-without-defaults TestDdf$NestedDefaults "
required_string: 'overridden required_string'
optional_with_default: 'overridden with_default'
optional_without_default: 'overridden without_default'
optional_message {
  optional_string: 'overridden optional_string'
  optional_int: 11
  optional_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  optional_enum: ENUM_VAL0
  optional_bool: false
}
repeated_message {
}
repeated_message {
  optional_string: 'overridden optional_string'
  optional_int: 11
  optional_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  optional_enum: ENUM_VAL0
  optional_bool: false
}
repeated_int: 0
repeated_int: 1")))
  (is (= {:optional-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :required-message {:required-string "overridden required_string"
                             :required-quat [1.0 2.0 3.0 4.0]
                             :required-enum :enum-val1}
          :repeated-message [{:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}
                             {:required-string "overridden required_string"
                              :required-quat [1.0 2.0 3.0 4.0]
                              :required-enum :enum-val1}]}
         (read-map-without-defaults TestDdf$NestedRequireds "
required_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
optional_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
repeated_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}
repeated_message {
  required_string: 'overridden required_string'
  required_quat {
    x: 1.0
    y: 2.0
    z: 3.0
    w: 4.0
  }
  required_enum: ENUM_VAL1
}")))
  (is (= {:optional-resource "/overridden optional_resource"}
         (read-map-without-defaults TestDdf$ResourceFields "optional_resource: '/overridden optional_resource'"))))

(deftest read-map-without-defaults-specified-equals-defaults-test
  (is (= {:required-string ""
          :repeated-message [{}
                             {}]
          :repeated-int [0
                         0]}
         (read-map-without-defaults TestDdf$NestedDefaults "
required_string: ''
optional_with_default: 'default'
optional_without_default: ''
optional_message {
  optional_string: 'default'
  optional_int: 10
  optional_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  optional_enum: ENUM_VAL1
  optional_bool: true
}
repeated_message {
}
repeated_message {
  optional_string: 'default'
  optional_int: 10
  optional_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  optional_enum: ENUM_VAL1
  optional_bool: true
}
repeated_int: 0
repeated_int: 0")))
  (is (= {:required-message {:required-string ""
                             :required-quat [0.0 0.0 0.0 1.0]
                             :required-enum :enum-val0}
          :repeated-message [{:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}
                             {:required-string ""
                              :required-quat [0.0 0.0 0.0 1.0]
                              :required-enum :enum-val0}]}
         (read-map-without-defaults TestDdf$NestedRequireds "
required_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
optional_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
repeated_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}
repeated_message {
  required_string: ''
  required_quat {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
  required_enum: ENUM_VAL0
}")))
  (is (= {:optional-resource "/default"}
         (read-map-without-defaults TestDdf$ResourceFields "optional_resource: '/default'"))))

(deftest read-map-without-defaults-resources
  (assert (= "" (protobuf/default TestDdf$ResourceSimple :image)))
  (assert (= "/default" (protobuf/default TestDdf$ResourceFields :optional-resource)))
  (is (= {}
         (read-map-without-defaults TestDdf$ResourceSimple "image: ''"))
      "Remove empty paths if equal to the default.")
  (is (= {:optional-resource ""}
         (read-map-without-defaults TestDdf$ResourceFields "optional_resource: ''"))
      "Keep empty paths if not equal to the default.")
  (is (= {:optional-resource "/default"}
         (read-map-without-defaults TestDdf$ResourceFields "optional_resource: '/default'"))
      "Keep non-empty paths even if equal to the default."))
