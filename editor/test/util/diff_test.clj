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

(ns util.diff-test
  (:require [clojure.test :refer :all]
            [util.diff :as diff]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest find-edits-test
  (testing "Nop-edits encompass document."
    (is (= {:left-lines [""]
            :right-lines [""]
            :edits []}
           (diff/find-edits "" "")))
    (is (= {:left-lines ["x"]
            :right-lines ["x"]
            :edits [{:type :nop
                     :left {:begin 0 :end 1}
                     :right {:begin 0 :end 1}}]}
           (diff/find-edits "x" "x")))
    (is (= {:left-lines ["1"
                         "2"]
            :right-lines ["1"
                          "2"]
            :edits [{:type :nop
                     :left {:begin 0 :end 2}
                     :right {:begin 0 :end 2}}]}
           (diff/find-edits "1\n2" "1\n2"))))

  (testing "Newlines are considered to be part of the prior line."
    (is (= {:left-lines [""]
            :right-lines [""
                          ""]
            :edits [{:type :insert
                     :left {:begin 0 :end 0}
                     :right {:begin 0 :end 1}}]}
           (diff/find-edits "" "\n")))
    (is (= {:left-lines ["x"]
            :right-lines ["x"
                          ""]
            :edits [{:type :replace
                     :left {:begin 0 :end 1}
                     :right {:begin 0 :end 1}}]}
           (diff/find-edits "x" "x\n"))))

  (testing "Non-trivial edits."
    (is (= {:left-lines ["1"
                         "2"
                         "3"
                         ""]
            :right-lines ["1"
                          "2"
                          "3"
                          "4"
                          ""]
            :edits [{:type :nop
                     :left {:begin 0 :end 3}
                     :right {:begin 0 :end 3}}
                    {:type :insert
                     :left {:begin 3 :end 3}
                     :right {:begin 3 :end 4}}]}
           (diff/find-edits "1\n2\n3\n" "1\n2\n3\n4\n")))
    (is (= {:left-lines ["1"
                         "2"
                         "3"
                         ""]
            :right-lines ["4"
                          "1"
                          "2"
                          "3"
                          ""]
            :edits [{:type :insert
                     :left {:begin 0 :end 0}
                     :right {:begin 0 :end 1}}
                    {:type :nop
                     :left {:begin 0 :end 3}
                     :right {:begin 1 :end 4}}]}
           (diff/find-edits "1\n2\n3\n" "4\n1\n2\n3\n")))
    (is (= {:left-lines ["1"
                         "2"
                         "3"
                         ""]
            :right-lines ["2"
                          "3"
                          ""]
            :edits [{:type :delete
                     :left {:begin 0 :end 1}
                     :right {:begin 0 :end 0}}
                    {:type :nop
                     :left {:begin 1 :end 3}
                     :right {:begin 0 :end 2}}]}
           (diff/find-edits "1\n2\n3\n" "2\n3\n")))))
