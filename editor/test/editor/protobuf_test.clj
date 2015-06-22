(ns editor.protobuf-test
  (:require [clojure.test :refer :all]
            [editor.protobuf :as protobuf])
  (:import [TestScript TestDdf TestDdf$Msg TestDdf$SubMsg TestDdf$Transform TestDdf$DefaultValue
            TestDdf$OptionalNoDefaultValue TestDdf$EmptyMsg TestDdf$Uint64Msg]
           [javax.vecmath Point3d Vector3d]))

(deftest simple
  (let [m {:uint-value 1}
        msg (protobuf/map->pb TestDdf$SubMsg m)
        new-m (protobuf/pb->map msg)]
    (is (= 1 (.getUintValue msg)))
    (is (= m new-m))))

(deftest transform
  (let [m {:position [0.0 1.0 2.0]
           :rotation [0.0 1.0 2.0 3.0]}
        msg (protobuf/map->pb TestDdf$Transform m)
        new-m (protobuf/pb->map msg)]
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
           :bool-value true}
        msg (protobuf/map->pb TestDdf$Msg m)
        new-m (protobuf/pb->map msg)]
    (is (= 1 (.getUintValue msg)))
    (is (= 2 (.getIntValue msg)))
    (is (= "three" (.getStringValue msg)))
    (is (= m new-m))))

(deftest default-vals
  (let [defaults {:uint-value 10
                  :string-value "test"
                  :quat-value [0.0 0.0 0.0 1.0]
                  :enum-value :enum-val1
                  :bool-value true}
        msg (protobuf/map->pb TestDdf$DefaultValue {})
        new-m (protobuf/pb->map msg)]
    (is (= defaults new-m))))

(deftest optional-no-defaults
  (let [defaults {:uint-value 0
                  :string-value ""
                  :enum-value :enum-val0}
        msg (protobuf/map->pb TestDdf$OptionalNoDefaultValue {})
        new-m (protobuf/pb->map msg)]
    (is (= defaults new-m))))

(deftest empty-msg
  (let [m {}
        msg (protobuf/map->pb TestDdf$EmptyMsg m)
        new-m (protobuf/pb->map msg)]
    (is (= m new-m))))

(deftest uint64-msg
  (let [m {:uint64-value 1}
        msg (protobuf/map->pb TestDdf$Uint64Msg m)
        new-m (protobuf/pb->map msg)]
    (is (= m new-m))))

(uint64-msg)