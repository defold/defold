;; Copyright 2020 The Defold Foundation
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

(ns editor.diff-view-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.diff-view :as diff-view]))

(deftest diff
  (are [expect actual] (= expect actual)
       [{:left {:begin 0, :end 1}, :right {:begin 0, :end 1}, :type :nop}]
       (diff-view/diff "" "")

       [{:left {:begin 0, :end 1}, :right {:begin 0, :end 1}, :type :nop}]
       (diff-view/diff "x" "x")

       [{:left {:begin 0, :end 3}, :right {:begin 0, :end 3}, :type :nop}
        {:left {:begin 3, :end 3}, :right {:begin 3, :end 4}, :type :insert}]
       (diff-view/diff "1\n2\n3\n" "1\n2\n3\n4\n")

       [{:left {:begin 0, :end 0}, :right {:begin 0, :end 1}, :type :insert}
        {:left {:begin 0, :end 3}, :right {:begin 1, :end 4}, :type :nop}]
       (diff-view/diff "1\n2\n3\n" "4\n1\n2\n3\n")

       [{:left {:begin 0, :end 1}, :right {:begin 0, :end 0}, :type :delete}
        {:left {:begin 1, :end 3}, :right {:begin 0, :end 2}, :type :nop}]
       (diff-view/diff "1\n2\n3\n" "2\n3\n")))
