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

(ns editor.ui.fuzzy-choices
  (:require [cljfx.fx.text :as fx.text]
            [cljfx.fx.text-flow :as fx.text-flow]
            [clojure.core.reducers :as r]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.util :as util]
            [util.bit-set :as bit-set]
            [util.thread-util :as thread-util])
  (:import [javafx.scene.text Text TextFlow]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- option->fuzzy-matched-option [option->text prepared-pattern option]
  (when-some [[score matching-indices] (fuzzy-text/match-path prepared-pattern (option->text option))]
    (vary-meta option assoc :score score :matching-indices matching-indices)))

(defn- option-order [option->text a b]
  (let [a-meta (meta a)
        b-meta (meta b)
        score-comparison (compare (:score a-meta) (:score b-meta))]
    (if-not (zero? score-comparison)
      score-comparison
      (let [a-matching-indices (:matching-indices a-meta)
            b-matching-indices (:matching-indices b-meta)
            a-matched-substring-length (- (bit-set/last-set-bit a-matching-indices) (bit-set/first-set-bit a-matching-indices))
            b-matched-substring-length (- (bit-set/last-set-bit b-matching-indices) (bit-set/first-set-bit b-matching-indices))
            matched-substring-length-comparison (compare a-matched-substring-length b-matched-substring-length)]
        (if-not (zero? matched-substring-length-comparison)
          matched-substring-length-comparison
          (let [^String a-text (option->text a)
                ^String b-text (option->text b)
                text-comparison (.compare util/natural-order a-text b-text)]
            (if-not (zero? text-comparison)
              text-comparison
              (try
                (compare a b)
                (catch ClassCastException _ ; since value part of option may contain non-Comparable values (f.i. PersistentHashMap)
                  (compare (System/identityHashCode a) (System/identityHashCode b)))))))))))

(defn filter-options [option->matched-text option->label-text filter-text options]
  (let [prepared-pattern (fuzzy-text/prepare-pattern filter-text)]
    (if (fuzzy-text/empty-prepared-pattern? prepared-pattern)
      options
      (let [fuzzy-matched-options
            (->> options
                 (r/take-while (thread-util/thread-uninterrupted-predicate))
                 (r/map #(option->fuzzy-matched-option option->matched-text prepared-pattern %))
                 (r/filter some?)
                 (r/foldcat))]

        (thread-util/throw-if-interrupted!)
        (sort (partial option-order option->label-text)
              (seq fuzzy-matched-options))))))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [^String text matching-indices]
  (let [/ (inc (.lastIndexOf text "/"))]
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
          (fuzzy-text/runs (.length text) matching-indices))))

(defn make-matched-text-flow
  ^TextFlow [text matching-indices]
  (TextFlow. (into-array Text (matched-text-runs text matching-indices))))

(defn matching-indices [fuzzy-matched-option]
  (:matching-indices (meta fuzzy-matched-option)))

(defn- make-text-run-cljfx [text style-class]
  (cond-> {:fx/type fx.text/lifecycle
           :text text}
          style-class
          (assoc :style-class style-class)))

(defn- matched-text-runs-cljfx [^String text matching-indices]
  (let [/ (inc (.lastIndexOf text "/"))]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run-cljfx (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run-cljfx (subs text start /) "diminished")
                       (make-text-run-cljfx (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run-cljfx (subs text start end) "diminished")]

                      :else
                      [(make-text-run-cljfx (subs text start end) nil)])))
          (fuzzy-text/runs (.length text) matching-indices))))

(defn make-matched-text-flow-cljfx [text matching-indices & {:keys [deprecated]
                                                             :or {deprecated false}}]
  (cond->
    {:fx/type fx.text-flow/lifecycle
     :children (matched-text-runs-cljfx text matching-indices)}
    deprecated
    (assoc :style-class "deprecated")))
