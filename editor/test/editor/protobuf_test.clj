(ns dynamo.file.protobuf-test
  (:require [clojure.test :refer :all]
            [dynamo.file.protobuf :as protobuf])
  (:import [TestScript TestDdf TestDdf$Msg TestDdf$SubMsg TestDdf$Transform]
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

(types)
