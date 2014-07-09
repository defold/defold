(ns internal.node-test
  (:require [clojure.test :refer :all]
            [dynamo.types :refer [as-schema]]
            [internal.node :refer [deep-merge]]))

(def a-schema (as-schema {:names [java.lang.String]}))

(def m1 {:cached #{:derived}   :inputs {:simple-number 18 :another [:a] :nested ['v1] :setvalued #{:x}}})
(def m2 {:cached #{:expensive} :inputs {:simple-number 99 :another [:a] :nested ['v3] :setvalued #{:z}}})
(def expected-merge {:cached #{:expensive :derived},
                     :inputs {:simple-number 99
                              :another [:a :a],
                              :nested ['v1 'v3]
                              :setvalued #{:x :z}}})

(def s1           (assoc m1 :declared-type a-schema))
(def s2           (assoc m2 :declared-type a-schema))
(def schema-merge (assoc expected-merge :declared-type a-schema))


(deftest merging-nested-maps
  (is (= expected-merge (deep-merge m1 m2))))

(deftest merging-maps-with-schemas
  (is (= schema-merge (deep-merge s1 s2)))
  (is (:schema (meta (get (deep-merge s1 s2) :declared-type)))))
