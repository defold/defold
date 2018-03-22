(ns editor.ui.fuzzy-choices
  (:require [clojure.core.reducers :as r]
            [clojure.string :as string]
            [editor.fuzzy-text :as fuzzy-text])
  (:import (javafx.scene.text Text TextFlow)))

(set! *warn-on-reflection* true)

(defn- option->fuzzy-matched-option [option->text pattern option]
  (when-some [[score matching-indices] (fuzzy-text/match-path pattern (option->text option))]
    (with-meta option
               {:score score
                :matching-indices matching-indices})))

(defn- option-order [option->text a b]
  (let [score-comparison (compare (:score (meta b)) (:score (meta a)))]
    (if-not (zero? score-comparison)
      score-comparison
      (let [a-text (option->text a)
            b-text (option->text b)
            text-length-comparison (compare (count a-text) (count b-text))]
        (if-not (zero? text-length-comparison)
          text-length-comparison
          (let [text-comparison (compare a-text b-text)]
            (if-not (zero? text-comparison)
              text-comparison
              (try
                (compare a b)
                (catch ClassCastException _ ; since value part of option may contain non-Comparable values (f.i. PersistentHashMap)
                  (compare (System/identityHashCode a) (System/identityHashCode b)))))))))))

(defn filter-options [option->matched-text option->label-text filter-text options]
  (if (empty? filter-text)
    options
    (sort (partial option-order option->label-text)
          (seq (r/foldcat (r/filter some?
                                    (r/map (partial option->fuzzy-matched-option option->matched-text filter-text)
                                           options)))))))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [text matching-indices]
  (let [/ (or (some-> text (string/last-index-of \/) inc) 0)]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run (subs text start /) "diminished")
                       (make-text-run (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run (subs text start end) "diminished")]

                      :else
                      [(make-text-run (subs text start end) nil)])))
          (fuzzy-text/runs (count text) matching-indices))))

(defn make-matched-text-flow
  ^TextFlow [text matching-indices]
  (TextFlow. (into-array Text (matched-text-runs text matching-indices))))

(defn matching-indices [fuzzy-matched-option]
  (:matching-indices (meta fuzzy-matched-option)))
