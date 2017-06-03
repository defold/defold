(ns editor.fuzzy-text-test
  (:require [clojure.test :refer :all]
            [editor.fuzzy-text :as fuzzy-text]))

(deftest runs-test
  (are [length matching-indices expected]
    (is (= expected (editor.fuzzy-text/runs length matching-indices)))

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
