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
  (testing "Wide string literal"
    (is (= [[0 "punctuation.definition.string.begin.cpp"]
            [2 "string.quoted.double.cpp"]
            [5 "punctuation.definition.string.end.cpp"]
            [6 "source.cish"]]
           (analyze-runs "L\"str\"")))))
