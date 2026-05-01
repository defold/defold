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

(ns editor.fuzzy-text-reference
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [util.coll :refer [pair]]))

;; Reference implementation of the fuzzy text matching algorithm. This
;; implementation was used in the past but turned out to be too slow and memory
;; hungry in some real-world scenarios. We now use it as ground truth for the
;; tests covering the new algorithm.

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- case-insensitive-character-indices
  "Returns a vector of indices where the specified code point exists in the
  string. Both upper- and lower-case matches are returned."
  [^String string ^long ch ^long from-index]
  (assert (not (Character/isWhitespace ch)))
  (loop [from-index from-index
         indices (transient [])]
    (let [upper (.indexOf string (Character/toUpperCase ch) from-index)
          lower (.indexOf string (Character/toLowerCase ch) from-index)]
      (if (and (neg? upper) (neg? lower))
        (persistent! indices)
        (let [^long index (cond (neg? upper) lower
                                (neg? lower) upper
                                :else (min upper lower))]
          (recur (inc index)
                 (conj! indices index)))))))

(defn- whitespace-length
  "Counts the number of code points that represent whitespace in a string,
  starting at from-index and counting until the next non-whitespace code point
  or until it reaches string-length."
  ^long [^String string ^long string-length ^long from-index]
  (loop [index from-index
         whitespace-length 0]
    (if (or (= string-length index)
            (not (Character/isWhitespace (.codePointAt string index))))
      whitespace-length
      (recur (inc index)
             (inc whitespace-length)))))

(defn- matching-index-permutations
  "Returns a sequence of all permutations of indices inside the string where the
  pattern characters appear in order. I.e. 'abc' appears in 'abbc' as [0 1 3]
  and [0 2 3]. The from-index parameter can be used to limit the starting point
  in the string."
  [^String pattern ^String string ^long from-index]
  (when-not (or (empty? string)
                (string/blank? pattern))
    (let [pattern (string/trim pattern)
          pattern-length (.length pattern)
          string-length (.length string)]
      (loop [pattern-index 1
             matching-index-permutations (mapv vector (case-insensitive-character-indices string (.codePointAt pattern 0) from-index))]
        (if (= pattern-length pattern-index)
          matching-index-permutations
          (let [pattern-whitespace-length (whitespace-length pattern pattern-length pattern-index)
                pattern-index (+ pattern-index pattern-whitespace-length)]
            (recur (inc pattern-index)
                   (into []
                         (mapcat (fn [matching-indices]
                                   (let [^long prev-matching-index (peek matching-indices)
                                         from-index (if (pos? pattern-whitespace-length)
                                                      (+ 2 prev-matching-index)
                                                      (inc prev-matching-index))]
                                     (when (not= string-length from-index)
                                       (mapv (partial conj matching-indices)
                                             (case-insensitive-character-indices string (.codePointAt pattern pattern-index) from-index))))))
                         matching-index-permutations))))))))

(defn- every-character-is-letter-or-digit?
  "Returns true if every code point within the specified half-open range
  represent either a letter or digit."
  [^String string ^long from-index ^long to-index]
  (cond (= to-index from-index) true
        (not (Character/isLetterOrDigit (.codePointAt string from-index))) false
        :else (recur string (inc from-index) to-index)))

(defn- any-character-is-upper-case?
  "Returns true if any code point within the specified half-open range represent
  an upper-case character."
  [^String string ^long from-index ^long to-index]
  (cond (= to-index from-index) false
        (Character/isUpperCase (.codePointAt string from-index)) true
        :else (recur string (inc from-index) to-index)))

(defn- prev-boundary-index
  "Finds the previous boundary in a string. Starts at range-end (exclusive) and
  iterates over each code point until it reaches a boundary character or
  just past range-start. This means -1 is returned for no match if range-start
  is zero. Used for scoring."
  ^long [^String string ^long range-start ^long range-end]
  (let [index (dec range-end)]
    (cond (< index range-start) index
          (Character/isLetterOrDigit (.codePointAt string index)) (recur string range-start index)
          :else index)))

(defn- score
  "Scores the matching indices against the string that produced them. A lower
  score is better. The from-index parameter can be used to limit the starting
  point in the string. Typically several sequences of matching indices are
  obtained with the matching-index-permutations function, then scored here."
  ^long [^String string ^long from-index matching-indices]
  (assert (not (empty? matching-indices)))
  (loop [matching-indices matching-indices
         prev-matching-index Long/MIN_VALUE
         prev-match-type nil
         streak-length 0
         score 0]
    (if (empty? matching-indices)
      score
      (let [matching-index (long (first matching-indices))]
        (if (= from-index matching-index)

          ;; We're matching the very first character in the considered string.
          ;; It does not count towards the score. This gives a very slight
          ;; scoring edge matches that include the start of the string.
          (recur (next matching-indices)
                 matching-index
                 :string-start
                 0
                 0)

          ;; The first matching character is at least one character in.
          ;; This means we can safely examine the character before the match.
          (let [before-matching-index (dec matching-index)
                after-prev-match-index (if (= Long/MIN_VALUE prev-matching-index) from-index (inc prev-matching-index))
                ch (.codePointAt string matching-index)
                prev-ch (.codePointAt string before-matching-index)
                match-type (cond
                             (and (Character/isUpperCase ch)
                                  (Character/isLetterOrDigit prev-ch)
                                  (not (Character/isUpperCase prev-ch)))
                             :camel-hump

                             (and (Character/isLetterOrDigit ch)
                                  (not (Character/isLetterOrDigit prev-ch)))
                             :word-boundary)
                streak? (or (= prev-matching-index before-matching-index)
                            (case match-type
                              nil false

                              :camel-hump
                              (and (case prev-match-type
                                     :string-start (Character/isUpperCase (.codePointAt string from-index))
                                     :camel-hump true
                                     false)
                                   (not (any-character-is-upper-case? string after-prev-match-index before-matching-index)))

                              :word-boundary
                              (case prev-match-type
                                (:string-start :word-boundary)
                                (every-character-is-letter-or-digit? string after-prev-match-index before-matching-index)
                                false)))
                streak-length (if streak? (inc streak-length) 0)]
            (recur (next matching-indices)
                   matching-index
                   match-type
                   streak-length
                   (if streak?
                     (if (< streak-length 2) (inc score) score)
                     (let [scored-range-start (if (= Long/MIN_VALUE prev-matching-index)
                                                (prev-boundary-index string from-index matching-index)
                                                prev-matching-index)
                           added-score (- matching-index scored-range-start)]
                       (+ score added-score))))))))))

(defn- best-match
  "Takes two matches returned from the match function and returns the one that
  is best match. Returns the second match if given nil as the first argument."
  [match-a [^long score-b :as match-b]]
  (let [^long score-a (if match-a (first match-a) Long/MAX_VALUE)]
    (if (<= score-a score-b)
      match-a
      match-b)))

(defn match
  "Performs a fuzzy text match against a string using the specified pattern.
  Returns a two-element vector of [score, matching-indices], or nil if the
  pattern is empty or there is no match. The matching-indices vector will
  contain the character indices in string that matched the pattern in sequential
  order. A lower score represents a better match."
  ([^String pattern ^String string]
   (match pattern string 0))
  ([^String pattern ^String string ^long from-index]
   (transduce (map (fn [matching-indices]
                     (pair (score string from-index matching-indices)
                           matching-indices)))
              (completing best-match)
              nil
              (matching-index-permutations pattern string from-index))))

(defn- apply-filename-bonus
  "Applies a bonus for a match on the filename part of a path."
  [^String path ^long basename-start [^long score matched-indices]]
  (let [basename-end (.lastIndexOf path ".")
        basename-end (if (neg? basename-end) (.length path) basename-end)
        basename-length (- basename-end basename-start)
        ^long basename-streak (loop [count 0]
                                (let [matched-index (get matched-indices count)]
                                  (if (and (not= matched-index basename-end)
                                           (= matched-index (+ count basename-start)))
                                    (recur (inc count))
                                    count)))
        basename-match-adjustment (if (pos? basename-streak)
                                    (quot (* 10 basename-streak) basename-length)
                                    0)]
    [(- score basename-match-adjustment 2) matched-indices]))

(defn match-path
  "Convenience function for matching against paths. The match function is
  called for the entire path as well as just the file name. We return the
  best-scoring match of the two, or nil if there was no match."
  [^String pattern ^String path]
  (when-some [path-match (match pattern path)]
    (if-some [last-slash-index (string/last-index-of path \/)]
      (let [name-index (inc ^long last-slash-index)]
        (if-some [name-match (some->> (match pattern path name-index) (apply-filename-bonus path name-index))]
          (best-match name-match path-match)
          path-match))
      path-match)))
