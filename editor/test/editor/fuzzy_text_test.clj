(ns editor.fuzzy-text-test
  (:require [clojure.test :refer :all]
            [editor.fuzzy-text :as fuzzy-text]))

(deftest match-test
  (are [pattern str str-matches]
    (let [expected-matching-indices (when str-matches
                                      (into []
                                            (comp (map-indexed vector)
                                                  (filter #(= \= (second %)))
                                                  (map first))
                                            str-matches))
          [_score matching-indices] (fuzzy-text/match-path pattern str)]
      (= expected-matching-indices matching-indices))
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
  (first (fuzzy-text/match pattern string)))

(defn- score-path [pattern proj-path]
  (first (fuzzy-text/match-path pattern proj-path)))

(deftest score-test
  (is (< (score-path "game.script" "/game/game.script")
         (score-path "game.script" "/game/score/score.gui_script")))
  (is (< (score-path "camera" "/utils/camera.lua")
         (score-path "camera" "/juego/com/king/juego/starlevel/app_star_level_game_round_api.lua"))))

(deftest selecta-conformance-tests
  ;; NOTE: Expected scores have been adjusted to account for the fact that we
  ;; don't count a match of the first character towards the score.

  (testing "Case is ignored"
    (is (some? (fuzzy-text/match "cA" "Case"))))

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
    (is (some? (fuzzy-text/match "ct" "cat")))
    (is (some? (fuzzy-text/match "ct" "Crate")))
    (is (nil? (fuzzy-text/match "ct" "tack"))))

  (testing "'le' will match 'lend' and 'ladder'."
    (is (some? (fuzzy-text/match "le" "lend")))
    (is (some? (fuzzy-text/match "le" "ladder")))
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

(deftest runs-test
  (are [length matching-indices expected]
    (is (= expected (fuzzy-text/runs length matching-indices)))

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
