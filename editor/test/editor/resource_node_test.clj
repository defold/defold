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

(ns editor.resource-node-test
  (:require [clojure.test :refer :all]
            [editor.resource-node :as resource-node]))

(set! *warn-on-reflection* true)

(defn- make-search-result-fn [search-fn pb-map]
  (fn search-result-fn [search-string]
    (let [match-fn (search-fn search-string)]
      (search-fn pb-map match-fn))))

(deftest default-ddf-resource-search-fn-test
  (let [save-value
        {:name "/sub-props.collection"
         :scale-along-z true

         :collection-instances
         [{:id "props"
           :collection "/props.collection"
           :position [0.0 12345.0 5.5]
           :scale [1.0 12.0 1.0]

           :instance-properties
           [{:id "props"

             :properties
             [{:id "props"

               :properties
               [{:id "atlas"
                 :type :property-type-hash
                 :value "/from-sub-props-collection.atlas"}]}]}]}]}

        search-matches
        (comp
          (partial mapv :value)
          (make-search-result-fn resource-node/default-ddf-resource-search-fn save-value))]

    (testing "Searching for text."
      (is (= ["/sub-props.collection"
              "/props.collection"
              "/from-sub-props-collection.atlas"]
             (search-matches "collection"))))

    (testing "Searching for numbers."
      (is (= [12345.0
              12.0]
             (search-matches "12"))))

    (testing "Searching for booleans."
      (is (= [true]
             (search-matches "rue"))))

    (testing "Searching for enum values."
      (is (= [:property-type-hash]
             (search-matches "PROPERTY_TYPE_"))))))

(deftest make-ddf-resource-search-fn-test
  (let [save-value
        {:name "/sub-props.collection"
         :scale-along-z true

         :collection-instances
         [{:id "props"
           :collection "/props.collection"
           :position [0.0 12345.0 5.5]
           :scale [12.0 1.0 1.0]

           :instance-properties
           [{:id "props"

             :properties
             [{:id "props"

               :properties
               [{:id "atlas"
                 :type :property-type-hash
                 :value "/from-sub-props-collection.atlas"}]}]}]}]}

        path-fn
        (fn path-fn [pb-map path]
          (is (identical? save-value pb-map))
          path)

        search-fn
        (resource-node/make-ddf-resource-search-fn [] path-fn)

        search-matches
        (comp
          (partial mapv (juxt :value :path))
          (make-search-result-fn search-fn save-value))]

    (testing "Searching for text."
      (is (= [["/sub-props.collection" [:name]]
              ["/props.collection" [:collection-instances 0 :collection]]
              ["/from-sub-props-collection.atlas" [:collection-instances 0 :instance-properties 0 :properties 0 :properties 0 :value]]]
             (search-matches "collection"))))

    (testing "Searching for numbers."
      (is (= [[12345.0 [:collection-instances 0 :position 1]]
              [12.0 [:collection-instances 0 :scale 0]]]
             (search-matches "12"))))

    (testing "Searching for booleans."
      (is (= [[true [:scale-along-z]]]
             (search-matches "rue"))))

    (testing "Searching for enum values."
      (is (= [[:property-type-hash [:collection-instances 0 :instance-properties 0 :properties 0 :properties 0 :type]]]
             (search-matches "PROPERTY_TYPE_"))))))
