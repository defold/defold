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

(ns editor.id-test
  (:require [clojure.test :refer :all]
            [editor.id :as id]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftest outline-resolve-id-test
  (testing "Single ids"
    (are [expected-id candidate-id taken-ids]
      (do (is (= expected-id (id/resolve candidate-id taken-ids)))
          (is (= [expected-id] (id/resolve-all [candidate-id] taken-ids))))

      "sprite" "sprite" #{""}
      "sprite2" "sprite2" #{"sprite"}
      "sprite1" "sprite" #{"sprite"}
      "sprite2" "sprite" #{"sprite" "sprite1"}
      "sprite2" "sprite1" #{"sprite" "sprite1"}
      "sprite1" "sprite" #{"sprite" "sprite2"}
      "sprite" "sprite1" #{"sprite1" "sprite2"}))

  (testing "Multiple ids"
    (is (= ["sprite" "sprite1" "sprite2"]
           (id/resolve-all ["sprite" "sprite" "sprite"] #{})))

    (is (= ["sprite3" "sprite4" "sprite5"]
           (id/resolve-all ["sprite" "sprite" "sprite"] #{"sprite" "sprite1" "sprite2"})))

    (is (= ["sprite3" "sprite4" "sprite5"]
           (id/resolve-all ["sprite1" "sprite2" "sprite3"] #{"sprite" "sprite1" "sprite2"})))))


(deftest id-test
  (doseq [[basename taken-ids expected]
          [["go" ["go"] #_=> "go1"]
           ["go" ["go1"] #_=> "go"]
           ["go" ["go" "go1" "go2"] #_=> "go3"]
           ["go" {"go" nil "go1" false} #_=> "go2"]
           ["go" #{"go" "foo" "go1"} #_=> "go2"]]]
    (is (= expected (id/gen basename taken-ids))))
  (doseq [[wanted-id taken-ids expected]
          [["go" ["go"] #_=> "go1"]
           ["go2" ["go"] #_=> "go2"]
           ["a1" #{"a" "a1"} #_=> "a2"]]]
    (is (= expected (id/resolve wanted-id taken-ids))))
  (doseq [[wanted-ids taken-ids expected]
          [[["go" "go" "go"] ["go" "go2"] #_=> ["go1" "go3" "go4"]]
           [["go" "go1" "go2"] ["go1"] #_=> ["go" "go2" "go3"]]
           [["a" "b" "b" "c2"] ["a" "a1" "b1"] #_=> ["a2" "b" "b2" "c2"]]]]
    (is (= expected (id/resolve-all wanted-ids taken-ids)))))
