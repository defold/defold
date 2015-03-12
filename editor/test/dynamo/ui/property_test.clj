(ns dynamo.ui.property-test
  (:require [clojure.test :refer :all]
            [dynamo.property :refer [defproperty]]
            [dynamo.types :as t]
            [internal.ui.property :as uip]))

(defproperty IntegerProperty            t/Int           (default 0))
(defproperty SpecializedIntegerProperty IntegerProperty (default -1))
(defproperty StringProperty             t/Str           (default ""))
(defproperty SpecializedStringProperty  StringProperty  (default "bork"))
(defproperty NonvisibleProperty         t/Keyword       (visible false))

(defn- pretend-property
  [prop]
  (zipmap [:type :value] prop))

(defn- pretend-node-properties
  [node-id props]
  (zipmap (keys props) (map #(assoc (pretend-property %) :node-id node-id) (vals props))))

(def props1 (pretend-node-properties 10 {:an-int [IntegerProperty 99] :a-string [StringProperty "first string"]}))
(def props2 (pretend-node-properties 15 {:an-int [IntegerProperty 99] :a-string [StringProperty "second string"]}))
(def props3 (pretend-node-properties 20 {:an-int [IntegerProperty 0]  :suppressed [NonvisibleProperty :internal-only]}))

#_(deftest property-aggregation
   (testing "like properties are merged"
     (let [aggregated (uip/aggregate-properties {:properties [props1 props2]})]
       (is (map? aggregated))
       (is (= #{:an-int}      (into #{} (keys aggregated))))
       (is (= 99              (get-in aggregated [:an-int :value])))
       (is (= IntegerProperty (get-in aggregated [:an-int :type])))
       (is (= #{10 15}        (into #{} (get-in aggregated [:an-int :node-id])))))))

(deftest only-visible-properties
  (let [aggregated (uip/aggregate-properties {:properties [props3]})]
    (is (map? aggregated))
    (is (not (contains? aggregated :suppressed)))
    (is (= #{:an-int} (into #{} (keys aggregated))))))
