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

(ns editor.resource-test
  (:require [clojure.test :refer :all]
            [editor.resource :as resource]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(deftest lines->proj-path-patterns-pred-test
  (testing "Patterns without the leading slash are ignored."
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["a"])]
      (is (false? (proj-path-patterns-pred "/a")))))

  (testing "Returns fn/constantly-false when there are no valid patterns."
    (is (identical? fn/constantly-false (resource/lines->proj-path-patterns-pred nil)))
    (is (identical? fn/constantly-false (resource/lines->proj-path-patterns-pred [])))
    (is (identical? fn/constantly-false (resource/lines->proj-path-patterns-pred ["a"])))
    (is (identical? fn/constantly-false (resource/lines->proj-path-patterns-pred ["a" "b"]))))

  (testing "Returns functioning predicate when there are some valid patterns."
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["a" "/b"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/b")))))

  (testing "Matches multiple patterns."
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/a" "/b"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/b")))
      (is (false? (proj-path-patterns-pred "/c")))))

  (testing "Path match rules."
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/A"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/A"))))
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/a"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (false? (proj-path-patterns-pred "/A"))))
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/a"])]
      (is (true? (proj-path-patterns-pred "/a")))
      (is (true? (proj-path-patterns-pred "/a/b")))
      (is (false? (proj-path-patterns-pred "/ab")))
      (is (false? (proj-path-patterns-pred "/aba")))
      (is (false? (proj-path-patterns-pred "/ab/a"))))
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/ab"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (false? (proj-path-patterns-pred "/a/b")))
      (is (true? (proj-path-patterns-pred "/ab")))
      (is (false? (proj-path-patterns-pred "/aba")))
      (is (true? (proj-path-patterns-pred "/ab/a"))))
    (let [proj-path-patterns-pred (resource/lines->proj-path-patterns-pred ["/ab/"])]
      (is (false? (proj-path-patterns-pred "/a")))
      (is (false? (proj-path-patterns-pred "/a/b")))
      (is (true? (proj-path-patterns-pred "/ab")))
      (is (false? (proj-path-patterns-pred "/aba")))
      (is (true? (proj-path-patterns-pred "/ab/a"))))))
