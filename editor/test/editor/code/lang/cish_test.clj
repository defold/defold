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
