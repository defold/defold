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

(ns internal.connection-rules
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer :all]))

(g/defnode InputNode
  (input string-scalar g/Str))

(g/defnode OutputNode
  (property string-scalar g/Str)
  (property int-scalar g/Int))

(deftest connection-rules
  (testing "compatible single value connections"
    (with-clean-system
      (let [[output input] (tx-nodes (g/make-node world OutputNode)
                                     (g/make-node world InputNode))]
        (g/connect! output :string-scalar input :string-scalar)
        (is (g/connected? (g/now) output :string-scalar input :string-scalar)))))

  (testing "incompatible single value connections"
    (with-clean-system
      (let [[output input] (tx-nodes (g/make-node world OutputNode)
                                     (g/make-node world InputNode))]
        (is (thrown? AssertionError
                     (g/connect! output :int-scalar input :string-scalar)))
        (is (not (g/connected? (g/now) output :string-scalar input :string-scalar)))))))

(deftest uni-connection-replaced
  (testing "single transaction"
    (with-clean-system
      (let [[out in next-out] (tx-nodes
                                (g/make-nodes world [out [OutputNode :string-scalar "first-val"]
                                                     in InputNode
                                                     next-out [OutputNode :string-scalar "second-val"]]
                                  (g/connect out :string-scalar in :string-scalar)
                                  (g/connect next-out :string-scalar in :string-scalar)))]
        (is (= "second-val" (g/node-value in :string-scalar))))))
  (testing "separate transactions"
    (with-clean-system
      (let [[out in next-out] (tx-nodes
                                (g/make-nodes world [out [OutputNode :string-scalar "first-val"]
                                                     in InputNode
                                                     next-out [OutputNode :string-scalar "second-val"]]
                                  (g/connect out :string-scalar in :string-scalar)))]
        (g/connect! next-out :string-scalar in :string-scalar)
        (is (= "second-val" (g/node-value in :string-scalar)))))))
