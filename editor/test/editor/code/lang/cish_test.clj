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

(ns editor.code.lang.cish-test
  (:require [clojure.test :refer :all]
            [editor.code.lang.cish :refer [grammar]]
            [editor.code.syntax :as syntax]))

(defn- analyze-runs [line]
  (let [contexts (list (syntax/make-context (:scope-name grammar) (:patterns grammar)))]
    (second (syntax/analyze contexts line))))

(deftest string-literals-test
  (testing "String literal"
    (is (= [[0 "punctuation.definition.string.begin.c"]
            [1 "string.quoted.double.c"]
            [4 "punctuation.definition.string.end.c"]
            [5 "source.cish"]]
           (analyze-runs "\"str\""))))

  (testing "Prefixed string literals"
    (are [line]
      (= [[0 "punctuation.definition.string.begin.cpp"]
          [2 "string.quoted.double.cpp"]
          [5 "punctuation.definition.string.end.cpp"]
          [6 "source.cish"]]
         (analyze-runs line))

      "L\"str\""
      "u\"str\""
      "U\"str\"")

    (is (= [[0 "punctuation.definition.string.begin.cpp"]
            [3 "string.quoted.double.cpp"]
            [6 "punctuation.definition.string.end.cpp"]
            [7 "source.cish"]]
           (analyze-runs "u8\"str\""))))

  (testing "Raw string literals"
    (are [line]
      (= [[0 "punctuation.definition.string.begin.cpp"]
          [3 "string.quoted.double.cpp"]
          [8 "punctuation.definition.string.end.cpp"]
          [9 "source.cish"]]
         (analyze-runs line))

      "LR\"(str)\""
      "uR\"(str)\""
      "UR\"(str)\"")

    (is (= [[0 "punctuation.definition.string.begin.cpp"]
            [4 "string.quoted.double.cpp"]
            [9 "punctuation.definition.string.end.cpp"]
            [10 "source.cish"]]
           (analyze-runs "u8R\"(str)\"")))

    (is (= [[0 "punctuation.definition.string.begin.cpp"]
            [2 "string.quoted.double.cpp"]
            [7 "punctuation.definition.string.end.cpp"]
            [8 "source.cish"]]
           (analyze-runs "R\"(str)\"")))))
