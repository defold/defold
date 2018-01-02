(ns editor.protobuf-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.protobuf :as protobuf])
  (:import [com.google.protobuf ByteString]))

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

(deftest values-at-path-test
  ;; NOTE: Cases correspond to the examples for protobuf/resource-field-paths-raw.
  (are [path m result]
    (= result (protobuf/values-at-path m path))

    ;; Simple
    [:image]
    {:image "img"}
    ["img"]

    [:image]
    {:other "img"}
    []

    ;; Repeated
    [[:images]]
    {:images ["img1" "img2"]}
    ["img1" "img2"]

    [[:images]]
    {:others ["img1" "img2"]}
    []

    ;; SimpleNested
    [:simple :image]
    {:simple {:image "nested-img"}}
    ["nested-img"]

    [:simple :image]
    {:other {:image "nested-img"}}
    []

    [:simple :image]
    {:simple {:other "nested-img"}}
    []

    ;; RepeatedNested
    [:simple [:images]]
    {:simple {:images ["nested-img1" "nested-img2"]}}
    ["nested-img1" "nested-img2"]

    [:simple [:images]]
    {:other {:images ["nested-img1" "nested-img2"]}}
    []

    [:simple [:images]]
    {:simple {:others ["nested-img1" "nested-img2"]}}
    []

    ;; SimpleRepeatedlyNested
    [[:simples] :image]
    {:simples [{:image "nested-img1"} {:image "nested-img2"}]}
    ["nested-img1" "nested-img2"]

    [[:simples] :image]
    {:others [{:image "nested-img1"} {:image "nested-img2"}]}
    []

    [[:simples] :image]
    {:simples [{:other "nested-img1"} {:other "nested-img2"}]}
    []

    ;; RepeatedRepeatedlyNested
    [[:repeateds] [:images]]
    {:repeateds [{:images ["nested-img1" "nested-img2"]} {:images ["nested-img3" "nested-img4"]}]}
    ["nested-img1" "nested-img2" "nested-img3" "nested-img4"]

    [[:repeateds] [:images]]
    {:others [{:images ["nested-img1" "nested-img2"]} {:images ["nested-img3" "nested-img4"]}]}
    []

    [[:repeateds] [:images]]
    {:repeateds [{:others ["nested-img1" "nested-img2"]} {:others ["nested-img3" "nested-img4"]}]}
    []))

(deftest update-values-at-path-test
  ;; NOTE: Cases correspond to the examples for protobuf/resource-field-paths-raw.
  (are [path before after]
    (= after (protobuf/update-values-at-path before path str "-suffix"))

    ;; Simple
    [:image]
    {:image "img"}
    {:image "img-suffix"}

    [:image]
    {:other "img"}
    {:other "img"}

    ;; Repeated
    [[:images]]
    {:images ["img1" "img2"]}
    {:images ["img1-suffix" "img2-suffix"]}

    [[:images]]
    {:others ["img1" "img2"]}
    {:others ["img1" "img2"]}

    ;; SimpleNested
    [:simple :image]
    {:simple {:image "nested-img"}}
    {:simple {:image "nested-img-suffix"}}

    [:simple :image]
    {:other {:image "nested-img"}}
    {:other {:image "nested-img"}}

    [:simple :image]
    {:simple {:other "nested-img"}}
    {:simple {:other "nested-img"}}

    ;; RepeatedNested
    [:simple [:images]]
    {:simple {:images ["nested-img1" "nested-img2"]}}
    {:simple {:images ["nested-img1-suffix" "nested-img2-suffix"]}}

    [:simple [:images]]
    {:other {:images ["nested-img1" "nested-img2"]}}
    {:other {:images ["nested-img1" "nested-img2"]}}

    [:simple [:images]]
    {:simple {:others ["nested-img1" "nested-img2"]}}
    {:simple {:others ["nested-img1" "nested-img2"]}}

    ;; SimpleRepeatedlyNested
    [[:simples] :image]
    {:simples [{:image "nested-img1"} {:image "nested-img2"}]}
    {:simples [{:image "nested-img1-suffix"} {:image "nested-img2-suffix"}]}

    [[:simples] :image]
    {:others [{:image "nested-img1"} {:image "nested-img2"}]}
    {:others [{:image "nested-img1"} {:image "nested-img2"}]}

    [[:simples] :image]
    {:simples [{:other "nested-img1"} {:other "nested-img2"}]}
    {:simples [{:other "nested-img1"} {:other "nested-img2"}]}

    ;; RepeatedRepeatedlyNested
    [[:repeateds] [:images]]
    {:repeateds [{:images ["nested-img1" "nested-img2"]} {:images ["nested-img3" "nested-img4"]}]}
    {:repeateds [{:images ["nested-img1-suffix" "nested-img2-suffix"]} {:images ["nested-img3-suffix" "nested-img4-suffix"]}]}

    [[:repeateds] [:images]]
    {:others [{:images ["nested-img1" "nested-img2"]} {:images ["nested-img3" "nested-img4"]}]}
    {:others [{:images ["nested-img1" "nested-img2"]} {:images ["nested-img3" "nested-img4"]}]}

    [[:repeateds] [:images]]
    {:repeateds [{:others ["nested-img1" "nested-img2"]} {:others ["nested-img3" "nested-img4"]}]}
    {:repeateds [{:others ["nested-img1" "nested-img2"]} {:others ["nested-img3" "nested-img4"]}]}))
