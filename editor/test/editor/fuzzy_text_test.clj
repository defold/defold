(ns editor.fuzzy-text-test
  (:require [clojure.test :refer :all]
            [editor.fuzzy-text :as fuzzy-text]))

(deftest match-test
  (are [pattern str str-matches]
    (let [expected-matching-indices (into []
                                          (comp (map-indexed vector)
                                                (filter #(= \= (second %)))
                                                (map first))
                                          str-matches)
          [_score matching-indices] (fuzzy-text/match pattern str)]
      (is (= expected-matching-indices matching-indices)))

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
    "/builtins/fonts/system_font.fp"
    "                       ===="

    ;; Non-consecutive matches.
    "secure"
    "consecutive-reaction"
    "   ====     =="

    ;; Case insensitive.
    "FONT"
    "/builtins/fonts/system_font.fp"
    "                       ===="

    ;; Matches against separator chars.
    "fsm"
    "/game/file_system_manager.lua"
    "      =    =      ="

    ;; Matches against camel humps.
    "FSM"
    "/game/FileSystemManager.md"
    "      =   =     ="

    ;; Space separates queries.
    "two words"
    "two_words"
    "=== ====="))

(defn- score [pattern proj-path]
  (first (fuzzy-text/match-path pattern proj-path)))

(deftest score-test
  (is (> (score "game.script" "/game/game.script")
         (score "game.script" "/game/score/score.gui_script")))
  (is (> (score "camera" "/utils/camera.lua")
         (score "camera" "/juego/com/king/juego/starlevel/app_star_level_game_round_api.lua"))))

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
