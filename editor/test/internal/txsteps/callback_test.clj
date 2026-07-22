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

(ns internal.txsteps.callback-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

(deftest callback-test
  (test-support/with-clean-system
    (let [calls (atom [])]
      (testing "Transact."
        (g/transact
          (g/callback
            (fn callback-fn [& args]
              (swap! calls conj (vec args)))
            :first-arg
            :second-arg))
        (is (= [[:first-arg :second-arg]] @calls))
        (is (= 0 (g/undo-stack-count :undo/global)))))))
