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

(ns internal.txsteps.update-graph-value-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest update-graph-value-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)]
      (testing "Before transact."
        (is (= nil (g/graph-value graph-id :things))))

      (testing "Transact."
        (g/transact
          (g/update-graph-value graph-id :things assoc :a 1))
        (is (= {:a 1} (g/graph-value graph-id :things))))

      (testing "Undo."
        (g/undo! graph-id)
        (is (= nil (g/graph-value graph-id :things))))

      (testing "Redo."
        (g/redo! graph-id)
        (is (= {:a 1} (g/graph-value graph-id :things)))))))
