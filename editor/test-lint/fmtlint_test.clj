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

(ns ^{:clj-kondo/config '{:linters {:defold/prefer-coll-into-> {:level :off}}}} fmtlint-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer [deftest is testing]]
            [fmtlint :as fmtlint]
            [lint :as lint])
  (:import [java.io StringWriter]
           [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]))

;; Checks positions, including bindings on an initializer's last line.
(deftest binding-layout-test
  (is (= [{:row 1 :col 11 :message "Start each binding on its own line."}]
         (fmtlint/lint "(let [a 1 b 2] b)")))
  (is (= [2]
         (into [] (map :row) (fmtlint/lint "(loop [a (f\n  1) b 2] b)"))))
  (doseq [form ["let" "loop" "binding" "with-open" "with-local-vars"
                "clojure.core/let"]]
    (testing form
      (is (= 1 (count (fmtlint/lint (str "(" form " [a 1 b 2] b)")))))
      (is (= [] (fmtlint/lint (str "(" form " [a 1\n b 2] b)")))))))

;; Checks multiline sides, including destructuring and nested lets.
(deftest initializer-layout-test
  (doseq [source ["(let [result (if cached\n cached\n (load-result))] result)"
                  "(let [{:keys [x]\n :as result} value] result)"
                  "(let [[x\n y] value] x)"
                  "(let [^String text (str prefix\n suffix)] text)"
                  "(let [x #?(:clj 1\n :cljs 2)] x)"
                  "(let [result (let [cached\n value]\n cached)] result)"
                  "(let [result\n (let [value (if cached\n cached\n (load-result))]\n value)]\n result)"
                  "(clojure.core/let [x (f\n y)] x)"
                  "(for [x xs\n :let [y (f\n x)]] y)"]]
    (is (= 1 (count (fmtlint/lint source))) source)
    (is (= "Start the initializer on a new line when either side is multiline."
           (:message (first (fmtlint/lint source))))
        source))
  (doseq [source ["(let [result\n (if cached\n cached\n (load-result))] result)"
                  "(let [{:keys [x]\n :as result}\n value] result)"
                  "(let [result\n (let [cached value]\n cached)] result)"
                  "(let [result\n      (let [cached (lookup-cache key)]\n        (or cached (load-result)))]\n  result)"
                  "(let [result\n      (let [a-cached-value (lookup-cache key)]\n        (or a-cached-value (load-result)))]\n  result)"
                  "(let [result\n (let [cached value\n\n value\n (if cached\n cached\n (load-result))]\n value)]\n result)"
                  "(let [_ (f\n x)] nil)"
                  "(let [^String _ (f\n x)] nil)"]]
    (is (= [] (fmtlint/lint source)) source)))

;; Checks the inclusive right edge and permits compact bindings at deep columns.
(deftest initializer-column-test
  (is (= [] (fmtlint/lint "(let [preview-data 1] preview-data)")))
  (is (= [] (fmtlint/lint "(let [preview-image 1] preview-image)")))
  (is (= [] (fmtlint/lint (str "(let [x \"" (.repeat "a" 70) "\"] x)"))))
  (is (= ["Start the initializer on a new line when it extends past column 80."]
         (mapv :message (fmtlint/lint (str "(let [x \"" (.repeat "a" 71) "\"] x)")))))
  (is (= [] (fmtlint/lint (str "(let [_ \"" (.repeat "a" 100) "\"] nil)"))))
  (is (= [] (fmtlint/lint (str "(let [x\n \"" (.repeat "a" 100) "\"] x)"))))
  (is (= [] (fmtlint/lint "(let [x (let [y (let [z 1] z)] y)] x)"))))

;; Requires real blank lines around multiline initializers, including comments.
(deftest multiline-binding-separation-test
  (doseq [[source expected-rows]
          [["(let [a 1\n b\n (f\n     2)\n c 3] c)" [2 5]]
           ["(let [a\n (f\n  1)\n b\n (g\n  2)] b)" [4]]
           ["(let [a 1\n\n b\n (f\n     2)\n\n c 3] c)" []]
           ["(let [a\n (f\n  1)\n  \t\n b 2] b)" []]
           ["(let [a\n (f\n  1)\n ;; comment\n b 2] b)" [5]]
           ["(let [a\n (f\n\n  1)\n b 2] b)" [5]]
           ["(let [a\n (f\n  1)\n\n ;; comment\n b 2] b)" []]
           ["(let [a\n (f\n  1)] a)" []]
           ["(let [a 1\n b 2] b)" []]
           ["(let [[a\n b]\n values\n\n c 3] c)" []]]]
    (let [findings (fmtlint/lint source)]
      (is (= expected-rows (mapv :row findings)) source)
      (doseq [finding findings]
        (is (= "Separate bindings with a blank line when either binding is multiline."
               (:message finding)))))))

;; Counts the full binding span when the initializer itself occupies one line.
(deftest split-binding-separation-test
  (doseq [[source expected-rows]
          [["(let [a\n 1\n b 2] b)" [3]]
           ["(let [a 1\n b\n 2] b)" [2]]
           ["(let [a\n 1\n b\n 2] b)" [3]]
           ["(let [a\n 1\n\n b 2] b)" []]
           ["(let [a 1\n\n b\n 2] b)" []]
           ["(let [directory\n (make-dir)\n clean (file directory)\n bad (file directory)\n clean-source\n \"text\"\n bad-source \"text\"] nil)" [3 5 7]]]]
    (let [findings (fmtlint/lint source)]
      (is (= expected-rows (mapv :row findings)) source)
      (doseq [finding findings]
        (is (= "Separate bindings with a blank line when either binding is multiline."
               (:message finding)))))))

;; Ensures whole-file traversal covers top-level and nested binding expressions.
(deftest whole-file-test
  (is (= 3 (count (fmtlint/lint
                    "(ns demo)\n(let [a 1 b 2] b)\n(defn f [] (let [c 3 d 4] d))\n{:x (let [e 5 f 6] f)}")))))

;; Preserves destructuring and comprehensions and checks nested :let bindings.
(deftest destructuring-and-comprehensions-test
  (is (= [] (fmtlint/lint "(for [a xs :when (p a)] a)")))
  (is (= 1 (count (fmtlint/lint "(let [{:keys [a b]} x [c d] y] a)"))))
  (is (= 1 (count (fmtlint/lint "(for [a xs\n :when (p a)\n :let [b 1 c 2]\n d ys] d)"))))
  (is (= [] (fmtlint/lint "(doseq [[a b] xs\n :let [c 1\n d 2]\n :while (p c)] (f d))"))))

;; Avoids findings in quoted, discarded, and compact data.
(deftest data-test
  (doseq [source ["'(let [a 1 b 2] b)"
                  "(quote (let [a 1 b 2] b))"
                  "`(let [a 1 b 2] b)"
                  "#_(let [a 1 b 2] b)"
                  "{:width w :height h}"
                  "(foo a b\n :k v\n :opt 123)"
                  "(let [a 1, ; comment\n #_ignored b 2] b)"]]
    (is (= [] (fmtlint/lint source)) source)))

;; Checks reader branches and preserves dotimes' optional result.
(deftest reader-conditionals-test
  (is (= 1 (count (fmtlint/lint "(let [#?@(:clj [a 1 b 2] :cljs [a 1\n b 2])] b)"))))
  (is (= [] (fmtlint/lint "#?(:clj (let [a 1\n b 2] b)\n :cljs (let [a 1\n b 2] b))")))
  (is (= [] (fmtlint/lint "(let [a #?(:clj 1 :cljs 2)\n b 3] b)")))
  (is (= [] (fmtlint/lint "(dotimes [i n (f)] (println i))"))))

;; Checks entry boundaries in nested maps and reader branches.
(deftest map-layout-test
  (doseq [[source row col]
          [["{:x 1 :y 2\n :z 3}" 1 7]
           ["{:x (f\n  1) :y 2}" 2 6]
           ["#:point{:x 1\n :y 2 :z 3}" 2 7]
           ["(case value {:x 1\n :y 2 :z 3} :hit :miss)" 2 7]
           ["{:outer {:x 1\n         :y 2 :z 3}}" 2 15]
           ["{:x 1\n #?@(:clj [:y 2 :z 3] :cljs [:y 2\n                           :z 3])}" 2 17]]]
    (is (= [{:row row :col col :message "Start each entry in a multiline map on its own line."}]
           (fmtlint/lint source))
        source))
  (doseq [source ["{:width w :height h}"
                  "{:width w\n :height h}"
                  "{(f\n  x) (g x)\n k v}"
                  "{:x 1 ; comment\n #_ignored :y 2}"
                  "'{:x 1\n :y 2 :z 3}"
                  "#_{:x 1\n :y 2 :z 3}"]]
    (is (= [] (fmtlint/lint source)) source)))

;; Skips call layout while still checking maps and bindings inside a call.
(deftest nested-layout-test
  (is (= [] (fmtlint/lint "(f a\n b c)")))
  (is (= ["Start each entry in a multiline map on its own line."
          "Start each binding on its own line."]
         (mapv :message
               (fmtlint/lint "(f a\n {:x 1\n  :y 2 :z 3} (doseq [x xs y ys] (g x y)))")))))

;; Reports input errors without losing diagnostics for the remaining files.
(deftest input-errors-test
  (let [output (java.io.StringWriter.)

        status
        (binding [*out* output]
          (fmtlint/check-files ["does-not-exist.clj" "src/lint/fmtlint.clj"]))]
    (is (= 2 status))
    (is (re-find #"does-not-exist.clj: error:" (str output)))))

;; Exercises all library checks and guards against cljfmt exiting on findings.
(deftest unified-lint-test
  (let [directory
        (.toFile (Files/createTempDirectory "lint-test-" (make-array FileAttribute 0)))

        clean (io/file directory "clean.clj")
        bad (io/file directory "bad.clj")
        layout-bad (io/file directory "layout_bad.clj")

        clean-source
        "(ns clean)\n\n(defn total []\n  (let [a 1\n        b 2]\n    (+ a b)))\n"

        bad-source "(ns bad)\n(defn total []\n(let [a missing b 2] (+ a b)))\n"

        layout-source
        "(ns layout-bad)\n\n(defn coordinates []\n  {:x 1\n   :y 2 :z 3})\n"]
    (try
      (spit clean clean-source)
      (spit bad bad-source)
      (spit layout-bad layout-source)
      (doseq [[file expected-status] [[clean 0] [bad 1] [layout-bad 1]]]
        (let [output (StringWriter.)

              status
              (binding [*out* output
                        *err* output]
                (lint/check-files [(str file)]))

              diagnostics (str output)]
          (is (= expected-status status) diagnostics)
          (is (re-find #"Running fmtlint" diagnostics))
          (when (= file layout-bad)
            (is (re-find #"Running clj-kondo\nRunning cljfmt\nRunning fmtlint" diagnostics))
            (is (not (re-find #"(?m)^@@" diagnostics)))
            (is (re-find #"Start each entry in a multiline map" diagnostics)))
          (when (= file bad)
            (is (re-find #"Unresolved symbol: missing" diagnostics))
            (is (re-find #"(?m)^@@" diagnostics))
            (is (re-find #"Start each binding on its own line" diagnostics)))))
      (is (= clean-source (slurp clean)))
      (is (= bad-source (slurp bad)))
      (is (= layout-source (slurp layout-bad)))
      (finally
        (.delete clean)
        (.delete bad)
        (.delete layout-bad)
        (.delete directory)))))
