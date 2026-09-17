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

(ns editor.fuzzy-text-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.fuzzy-text-reference :as fuzzy-text-ref]
            [util.bit-set :as bit-set]
            [util.coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- match [^String pattern ^String string]
  (let [prepared-pattern (fuzzy-text/prepare-pattern pattern)]
    (fuzzy-text/match prepared-pattern string)))

(defn- match-path [^String pattern ^String path]
  (let [prepared-pattern (fuzzy-text/prepare-pattern pattern)]
    (fuzzy-text/match-path prepared-pattern path)))

(deftest match-test
  (are [pattern str str-matches]
    (let [expected-matching-indices (when str-matches
                                      (into []
                                            (comp (map-indexed vector)
                                                  (filter #(= \= (second %)))
                                                  (map first))
                                            str-matches))
          [_score matching-indices] (match-path pattern str)]
      (= expected-matching-indices (some-> matching-indices bit-set/indices)))
    ;; Empty string matches nothing.
    ""
    "text"
    nil

    ;; Whitespace-only matches nothing.
    " "
    "text"
    nil

    ;; Matches against filename.
    "image"
    "/game/title_screen/image.atlas"
    "                   ====="

    ;; Matches against path.
    "builtins"
    "/builtins/render.config"
    " ========"

    ;; Filename match is better than path match.
    "font"
    "/builtins/font/system_font.fp"
    "                      ===="

    ;; Non-consecutive matches.
    "secure"
    "consecutive-reaction"
    "   ====     =="

    ;; Case insensitive.
    "FONT"
    "font.fp"
    "===="

    ;; Matches against word boundaries.
    "fsm"
    "/game/file_system_manager.lua"
    "      =    =      ="

    ;; Matches against camel humps.
    "fsm"
    "/game/FileSystemManager.md"
    "      =   =     ="

    ;; Space separates queries.
    "two words"
    "two_words"
    "=== ====="

    ;; Separate queries do not match joined.
    "two words"
    "twowords"
    nil))

(defn- score [pattern string]
  (first (match pattern string)))

(defn- score-path [pattern proj-path]
  (first (match-path pattern proj-path)))

(deftest score-test
  (is (< (score-path "game.script" "/game/game.script")
         (score-path "game.script" "/game/score/score.gui_script")))
  (is (< (score-path "camera" "/utils/camera.lua")
         (score-path "camera" "/juego/com/king/juego/starlevel/app_star_level_game_round_api.lua"))))

(deftest selecta-conformance-tests
  ;; NOTE: Expected scores have been adjusted to account for the fact that we
  ;; don't count a match of the first character towards the score.

  (testing "Case is ignored"
    (is (some? (match "cA" "Case"))))

  (testing "The score is the length of the matching substring."
    (is (= 1 (score "su" "substring")))
    (is (= 2 (score "sb" "substring")))
    (is (= 3 (score "ss" "substring"))))

  (testing "Characters on word boundaries only contribute one to the length."
    (is (= 1 (score "am" "app/models"))))

  (testing "Sequential characters only contribute one to the length."
    (is (= 2 (score "spec" "search_spec.rb"))))

  (testing "Acronym matches only contribute one to the length."
    (is (= 1 (score "amu" "app/models/user.rb"))))

  (testing "'ct' will match 'cat' and 'Crate', but not 'tack'."
    (is (some? (match "ct" "cat")))
    (is (some? (match "ct" "Crate")))
    (is (nil? (match "ct" "tack"))))

  (testing "'le' will match 'lend' and 'ladder'."
    (is (some? (match "le" "lend")))
    (is (some? (match "le" "ladder")))
    (testing "'lend' will appear higher in the results because the matching substring is shorter."
      (is (< (score "le" "lend")
             (score "le" "ladder"))))))

(deftest extended-conformance-tests
  (testing "Matching the first character do not count towards the score."
    (is (= 0 (score "a" "abstract_tree")))
    (is (= 1 (score "t" "abstract_tree"))))

  (testing "Acronyms match against CamelHumps."
    (is (= 1 (score "amu" "AppModelUser"))))

  (testing "'sp' will give 'game/specs' a score of 2."
    (is (= 2 (score "sp" "game/specs"))))

  (testing "'sp' will give 'aspect' a score of 3."
    (is (= 3 (score "sp" "aspect"))))

  (testing "'at' will give 'abstract_tree' an ideal score."
    (is (= 1 (score "at" "abstract_tree"))))

  (testing "'at' will score 'abstract_syntax_tree' less than 'abstract_tree'."
    (is (< (score "at" "abstract_tree")
           (score "at" "abstract_syntax_tree")))))

(deftest empty-prepared-pattern?-test
  (is (true? (fuzzy-text/empty-prepared-pattern? (fuzzy-text/prepare-pattern ""))))
  (is (false? (fuzzy-text/empty-prepared-pattern? (fuzzy-text/prepare-pattern "a")))))

(deftest runs-test
  (are [length matching-indices expected]
    (is (= expected (fuzzy-text/runs length (bit-set/from matching-indices))))

    0 [] []
    1 [] [[false 0 1]]
    2 [] [[false 0 2]]
    1 [0] [[true 0 1]]
    2 [0] [[true 0 1] [false 1 2]]
    2 [1] [[false 0 1] [true 1 2]]
    2 [0 1] [[true 0 2]]
    3 [0 2] [[true 0 1] [false 1 2] [true 2 3]]
    4 [1 3] [[false 0 1] [true 1 2] [false 2 3] [true 3 4]]
    4 [0 1] [[true 0 2] [false 2 4]]
    4 [1 2] [[false 0 1] [true 1 3] [false 3 4]]
    4 [2 3] [[false 0 2] [true 2 4]]))

;; -----------------------------------------------------------------------------
;; Verify behavior against slower reference implementation.
;; -----------------------------------------------------------------------------

(defn- ref-match [pattern string]
  (when-some [[score matched-indices] (fuzzy-text-ref/match pattern string)]
    (pair score (bit-set/from (count matched-indices) matched-indices))))

(defspec match-impl-equates-to-reference-impl 10000
  (let [needle-char-gen
        (gen/frequency
          [[1 (gen/return \space)]
           [6 gen/char-alphanumeric]])

        needle-gen
        (gen/fmap
          string/join
          (gen/vector needle-char-gen 1 10))

        haystack-gen
        (gen/fmap
          string/join
          (gen/vector gen/char-alphanumeric 1 100))]

    (prop/for-all
      [needle needle-gen
       haystack haystack-gen]
      (= (ref-match needle haystack)
         (match needle haystack)))))

(defn- ref-match-path [pattern string]
  (when-some [[score matched-indices] (fuzzy-text-ref/match-path pattern string)]
    (pair score (bit-set/from (count matched-indices) matched-indices))))

(defspec match-path-impl-equates-to-reference-impl 10000
  (let [needle-char-gen
        (gen/frequency
          [[1 (gen/return \space)]
           [1 (gen/elements [\- \. \/ \_])]
           [5 gen/char-alphanumeric]])

        needle-gen
        (gen/fmap
          string/join
          (gen/vector needle-char-gen 1 10))

        path-segment-char-gen
        (gen/frequency
          [[1 (gen/elements [\- \. \_ \space])]
           [10 gen/char-alphanumeric]])

        path-segment-string-gen
        (gen/fmap
          string/join
          (gen/vector
            path-segment-char-gen 1 20))

        regular-path-segment-gen
        (gen/such-that
          #(not (#{"." ".."} %))
          path-segment-string-gen
          50)

        hidden-path-segment-gen
        (gen/fmap
          #(str \. %)
          regular-path-segment-gen)

        dot-path-segment-gen
        (gen/elements ["." ".."])

        path-segment-gen
        (gen/frequency
          [[1 dot-path-segment-gen]
           [2 hidden-path-segment-gen]
           [8 regular-path-segment-gen]])

        path-gen
        (gen/let [segments (gen/vector path-segment-gen 1 10)]
          (str \/ (string/join "/" segments)))]

    (prop/for-all
      [needle needle-gen
       haystack path-gen]
      (= (ref-match-path needle haystack)
         (match-path needle haystack)))))

(defn- highlight [[score matching-indices] ^String string]
  (let [length (.length string)
        ^StringBuilder sb (StringBuilder.)]
    (loop [index 0]
      (when (< index length)
        (let [is-match (bit-set/bit matching-indices index)]
          (when is-match
            (.append sb \[))
          (.appendCodePoint sb (.codePointAt string index))
          (when is-match
            (.append sb \]))
          (recur (inc index)))))
    {:text (.toString sb)
     :indices (bit-set/into (vector-of :int) matching-indices)
     :score score}))

;; -----------------------------------------------------------------------------

(deftest turkish-capital-i-test
  (is (= [0 (bit-set/of 0)] (match "İ" "İ")))
  (is (= [0 (bit-set/of 0)] (match "İ" "i"))))

(deftest no-freeze-test
  (let [needle "MYVARLONG"
        haystack "yyyZhniZzzzZbarAnmrAgneBlryCaveDroeGkerGrjuGuruGgnaHinaHrbeHariHadnKanaKooaLntaLmylMayrOlmaTuleTiahTtbiTopoBiarBsnaCrehCihtErmhKgnoMrmyMmagOrnuRhniScrySaahTiiiYtrsDhtoGlatIdhuBonaHglgTbgaTtrpCbmiLbniLamsOwahSelaTragUiguBtpoCgalGrahKulaToepXolySgnfTilaBxusXookNgahPxnhPiraCmahCilaKcpeLicyLidyLkclOgnjRruaSdnuSiiaVtsvAumaBpygEimrAilhPitrPavaJihtKusiLietMbraShkrOrmaSanaLtvaTktaBharBdnaMmkaCcreMoreMdrlPdrhSaroSrkaTssaBbhgAlpuDablEnarGjohKdniSaniLjhaMinaMdneMidoMoorMtabNbraNmrePgnmHmlaPcuaPplhPddiShriTaraWmohAwulHrtaHtluMgnuHwngSmldAskhBcraMegsOgnaTaweNmnoGuhsNoyoSbnaZrgoDgnoGghoRakaMfdeMogoSdgoSmylEdnaNpnmHohcWsrhCkaiDstiKizeYnmpCrguOasnTotoThtiVhtmZiwaKmgaN"
        [score matching-indices] (match needle haystack)]
    (is (= 129 score))
    (is (= (bit-set/of 17 26 29 50 56 75 87 113 128) matching-indices))))

(deftest past-failures-test
  (doseq [[needle haystack]
          [["XM" "X0mM000000"]
           ["GDA" "GdDA"]
           ["0 0" "0"]
           [" " "0"]
           ["J E" "J0eE"]
           ["M C" "0mM0C"]]]
    (let [expected (some-> (ref-match needle haystack) (highlight haystack))
          actual (some-> (match needle haystack) (highlight haystack))]
      (is (= expected actual)))))

(comment
  ;; Use to visualize differences when there are discrepancies.
  (let [[needle haystack] ["M C" "0mM0C"]]
    (let [old (ref-match needle haystack)
          new (match needle haystack)]
      {:expected (some-> old (highlight haystack))
       :actual (some-> new (highlight haystack))})))
