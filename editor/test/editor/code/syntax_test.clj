(ns editor.code.syntax-test
  (:require [clojure.test :refer :all]
            [editor.code.syntax :as syntax]))

(def ^:private simple-line
  "if my_variable == 3 then")

(def ^:private simple-patterns
  [{:match #"(?<![\d.])\s0x[a-fA-F\d]+|\b\d+(\.\d+)?([eE]-?\d+)?|\.\d+([eE]-?\d+)?"
    :name "constant.numeric.lua"}
   {:match #"\b(and|or|not|break|do|else|for|if|elseif|return|then|repeat|while|until|end|function|local|in|goto)\b"
    :name "keyword.control.lua"}
   {:match #"\+|-|%|#|\*|\/|\^|==?|~=|<=?|>=?|(?<!\.)\.{2}(?!\.)"
    :name "keyword.operator.lua"}])

(defn ^:private first-match [patterns line start]
  (let [contexts (list (syntax/make-context "source.lua" patterns))]
    (#'syntax/first-match contexts line start)))

(deftest first-match-test
  (are [offset pattern-index range]
    (let [match (first-match simple-patterns simple-line offset)]
      (when (is (some? match))
        (is (identical? (simple-patterns pattern-index) (:pattern match)))
        (is (= (first range) (.start (:match-result match))))
        (is (= (second range) (.end (:match-result match))))))

    0 1 [0 2]
    2 2 [15 17]
    17 0 [18 19]
    19 1 [20 24]))

(defn ^:private analyze-runs [scope-name patterns line]
  (let [contexts (list (syntax/make-context scope-name patterns))]
    (second (syntax/analyze contexts line))))

(deftest runs-test
  (testing "Simple operations"
    (is (= [[0 "keyword.control.lua"]
            [2 "source.lua"]
            [15 "keyword.operator.lua"]
            [17 "source.lua"]
            [18 "constant.numeric.lua"]
            [19 "source.lua"]
            [20 "keyword.control.lua"]
            [24 "source.lua"]]
           (analyze-runs "source.lua" simple-patterns simple-line))))

  (testing "Captures"
    (let [patterns [{:captures {1 {:name "keyword.control.lua"}
                                2 {:name "entity.name.function.scope.lua"}
                                3 {:name "entity.name.function.lua"}
                                4 {:name "punctuation.definition.parameters.begin.lua"}
                                5 {:name "variable.parameter.function.lua"}
                                6 {:name "punctuation.definition.parameters.end.lua"}}
                     :match #"\b(function)(?:\s+([a-zA-Z_.:]+[.:])?([a-zA-Z_]\w*)\s*)?(\()([^)]*)(\))"
                     :name "meta.function.lua"}]]
      (is (= [[0 "keyword.control.lua"]
              [8 "meta.function.lua"]
              [9 "entity.name.function.lua"]
              [13 "punctuation.definition.parameters.begin.lua"]
              [14 "variable.parameter.function.lua"]
              [32 "punctuation.definition.parameters.end.lua"]
              [33 "source.lua"]]
             (analyze-runs "source.lua" patterns "function push(position, velocity)")))
      (is (= [[0 "keyword.control.lua"]
              [8 "meta.function.lua"]
              [9 "entity.name.function.scope.lua"]
              [14 "entity.name.function.lua"]
              [18 "punctuation.definition.parameters.begin.lua"]
              [19 "variable.parameter.function.lua"]
              [37 "punctuation.definition.parameters.end.lua"]
              [38 "source.lua"]]
             (analyze-runs "source.lua" patterns "function Ball:push(position, velocity)")))))

  (testing "Begin captures"
    (let [patterns [{:begin #"(u|u8|U|L)?\""
                     :begin-captures {0 {:name "punctuation.definition.string.begin.cpp"}
                                      1 {:name "meta.encoding.cpp"}}
                     :end #"\""
                     :end-captures {0 {:name "punctuation.definition.string.end.cpp"}}
                     :name "string.quoted.double.cpp"}]]
      (is (= [[0 "punctuation.definition.string.begin.cpp"]
              [1 "string.quoted.double.cpp"]
              [4 "punctuation.definition.string.end.cpp"]
              [5 "source.cish"]]
             (analyze-runs "source.cish" patterns "\"str\"")))
      (is (= [[0 "meta.encoding.cpp"]
              [1 "punctuation.definition.string.begin.cpp"]
              [2 "string.quoted.double.cpp"]
              [5 "punctuation.definition.string.end.cpp"]
              [6 "source.cish"]]
             (analyze-runs "source.cish" patterns "L\"str\""))))))
