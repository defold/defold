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

(ns editor.collision-groups-test
  (:require [clojure.test :refer :all]
            [editor.collision-groups :as collision-groups]))

(set! *warn-on-reflection* true)

(deftest collision-group-color-test
  (is (= [1.0 1.0 1.0 1.0]
         (collision-groups/color nil)))
  (is (= [0.5 0.375 1.0 1.0]
         (collision-groups/color "player")))
  (is (not= (collision-groups/color "player")
            (collision-groups/color "enemy"))))

(deftest collision-group-overallocation-test
  (let [make-collision-groups
        (fn [group-count]
          (mapv (fn [group-index]
                  (str "group" group-index))
                (range group-count)))]
    (is (not (collision-groups/overallocated?
               (make-collision-groups collision-groups/MAX-GROUPS))))
    (is (collision-groups/overallocated?
          (make-collision-groups (inc collision-groups/MAX-GROUPS))))
    (is (not (collision-groups/overallocated?
               (conj (make-collision-groups collision-groups/MAX-GROUPS)
                     "group0"))))))
