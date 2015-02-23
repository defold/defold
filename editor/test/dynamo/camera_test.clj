(ns dynamo.camera-test
  (:require [dynamo.camera :refer :all]
            [dynamo.ui :refer :all]
            [clojure.test :refer :all]))

(deftest mouse-button-interpretation
  (testing "one-button-movements"
    (are [move button control alt shift] (= move (camera-movement :one-button button control alt shift))
         :track  1 true   true false
         :tumble 1 false  true false
         :dolly  1 true  false false
         :idle   1 false false false))
  (testing "three-button-movements"
    (are [move button control alt shift] (= move (camera-movement :three-button button control alt shift))
         :track  2 false  true false
         :tumble 1 false  true false
         :dolly  3 false  true false
         :idle   1 false false false
         :idle   3  true false false
         :idle   3 false false false
         :idle   2  true false false)))
