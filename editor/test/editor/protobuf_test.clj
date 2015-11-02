(ns editor.protobuf-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.test TestDdf TestDdf$Msg TestDdf$SubMsg TestDdf$Transform TestDdf$DefaultValue
            TestDdf$OptionalNoDefaultValue TestDdf$EmptyMsg TestDdf$Uint64Msg TestDdf$RepeatedUints
            TestDdf$NestedMessages TestDdf$NestedMessages$NestedEnum$Enum TestDdf$BooleanMsg TestDdf$BytesMsg
            TestAtlasProto$AtlasAnimation TestAtlasProto$AtlasImage]
           [javax.vecmath Point3d Vector3d]
           [com.google.protobuf ByteString]))

(defn- round-trip [^java.lang.Class cls m]
  (with-in-str (protobuf/map->str cls m)
    (protobuf/read-text cls (io/reader *in*))))

(defn- round-trip-data [^java.lang.Class cls s]
  (with-in-str s
    (protobuf/map->str cls (protobuf/read-text cls (io/reader *in*)))))

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
    (is (identical? java.lang.Boolean/FALSE (:bool-value new-m)))
    (is (= m new-m))))

(deftest default-vals
  (testing "Defaults readable"
           (let [defaults {:uint-value 10
                           :string-value "test"
                           :quat-value [0.0 0.0 0.0 1.0]
                           :enum-value :enum-val1
                           :bool-value true}
                 new-m (round-trip TestDdf$DefaultValue {})]
             (is (= defaults new-m))))
  (testing "Defaults overwritable"
           (let [m (round-trip TestDdf$DefaultValue {})
                 ks (keys m)]
             (doseq [k ks]
               (is (not (protobuf/field-set? m k))))
             (doseq [[k v] (map vector ks [11 "test2" [1.0 0.0 0.0 0.0] :enum-val0 false])]
               (is (protobuf/field-set? (assoc m k v) k)))
             (is (protobuf/field-set? (update m :quat-value assoc 0 1.0) :quat-value))))
  (testing "Defaults not saved"
           (let [data ""
                 new-data (round-trip-data TestDdf$DefaultValue data)]
             (is (= data new-data)))))

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
  (let [m {:msg {:enum :enum-val0}}
        new-m (round-trip TestDdf$NestedMessages m)]
    (is (= m new-m))))

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
