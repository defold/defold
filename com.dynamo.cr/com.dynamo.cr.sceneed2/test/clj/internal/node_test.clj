(ns internal.node-test
  (:require [clojure.test :refer :all]
            [internal.node :refer [deep-merge]]))

(def m1 {:cached #{:derived}   :inputs {:simple-number 18 :nested ['v1]}})
(def m2 {:cached #{:expensive} :inputs {:simple-number 99 :another ['v2] :nested ['v3]}})

(def expected-merge {:cached #{:expensive :derived}, :inputs {:simple-number 99 :another ['v2], :nested ['v1 'v3]}})

(deftest merging-nested-maps
  (is (= expected-merge (deep-merge m1 m2))))

