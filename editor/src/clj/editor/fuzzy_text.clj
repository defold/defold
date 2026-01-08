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

(ns editor.fuzzy-text
  (:require [clojure.string :as string]
            [util.bit-set :as bit-set]
            [util.coll :as coll :refer [pair]])
  (:import [java.util.concurrent.atomic AtomicInteger]))

;; Sublime Text-style fuzzy text matching.
;;
;; Based on the "Selecta" ranking algorithm by Gary Bernhardt:
;; https://github.com/garybernhardt/selecta
;;
;; The original "Selecta" ranking algorithm is:
;; - Select each input string that contains all of the characters in the query.
;;   They must be in order, but don't have to be sequential. Case is ignored.
;; - The score is the length of the matching substring. Lower is better.
;; - If a character is on a word boundary, it only contributes 1 to the length,
;;   rather than the actual number of characters since the previous matching
;;   character. Querying "app/models" for "am" gives a score of 2, not 5.
;; - Bonus for exact queries: when several adjacent characters match
;;   sequentially, only the first two score. Querying "search_spec.rb" for
;;   "spec" gives a score of 2, not 4.
;; - Bonus for acronyms: when several sequential query characters exist on word
;;   boundaries, only the first two score. Querying the string
;;   "app/models/user.rb" for "amu" gives a score of 2, not 3.
;;
;; Some concrete examples of the original "Selecta" ranking algorithm:
;; - "ct" will match "cat" and "Crate", but not "tack".
;; - "le" will match "lend" and "ladder". "lend" will appear higher in the
;;   results because the matching substring is shorter ("le" vs. "ladde").
;;
;; Differences from the original "Selecta" ranking algorithm:
;; - A match of the first character in the string does not count towards the
;;   score. We do this to prioritize matches against the start of the string.
;; - Our algorithm treats camel humps similar to word boundaries so that they
;;   can be matched using acronyms.
;; - If the first matching character is not on a camel hump or a word boundary,
;;   our algorithm scores it by its distance from the previous word boundary.
;; - Our algorithm penalizes acronyms that skip over a camel hump or word
;;   boundary in the input string.
;;
;; Some concrete examples of the differences with our algorithm:
;; - "sp" will give "game/specs" a score of 2, but "aspect" a score of 3 since
;;   you pay for every unmatched character before a word boundary.
;; - "at" will give "abstract_tree" an ideal score, since the acronym matches
;;   every word boundary.
;; - "at" will give "abstract_syntax_tree" a lesser score than "abstract_tree",
;;   since the acronym missed the 's' on the middle word boundary.

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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
  "Scores the matching-indices bit-set against the string that produced it. A
  lower score is better. The from-index parameter can be used to limit the
  starting point in the string. Typically several bit-sets of matching-indices
  are obtained from the matching-index-permutations function, then scored here."
  ^long [^String string ^long from-index matching-indices]
  (assert (not (bit-set/empty? matching-indices)))
  (loop [matching-index (bit-set/first-set-bit matching-indices)
         prev-matching-index Long/MIN_VALUE
         prev-match-type nil
         streak-length 0
         score 0]
    (cond
      (neg? matching-index)
      ;; We're done.
      score

      (= from-index matching-index)
      ;; We're matching the very first character in the considered string.
      ;; It does not count towards the score. This gives a very slight
      ;; scoring edge to matches that include the start of the string.
      (recur (bit-set/next-set-bit matching-indices (inc matching-index))
             matching-index
             :string-start
             0
             0)

      :else
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
        (recur (bit-set/next-set-bit matching-indices (inc matching-index))
               matching-index
               match-type
               streak-length
               (if streak?
                 (if (< streak-length 2) (inc score) score)
                 (let [scored-range-start (if (= Long/MIN_VALUE prev-matching-index)
                                            (prev-boundary-index string from-index matching-index)
                                            prev-matching-index)
                       added-score (- matching-index scored-range-start)]
                   (+ score added-score))))))))

(defn- best-match
  "Takes two matches returned from the match function and returns the one that
  is best match. Returns the second match if given nil as the first argument."
  [match-a [^long score-b :as match-b]]
  (let [^long score-a (if match-a (first match-a) Long/MAX_VALUE)]
    (if (<= score-a score-b)
      match-a
      match-b)))

(defn- set-matching-index-bits-in-bit-set!
  "Sets bits in the supplied bit-set to true at each index where the specified
  code point is found in the string."
  [bits ^String string ^long ch ^long from-index]
  (loop [from-index (int from-index)]
    (let [index (.indexOf string ch from-index)]
      (when-not (neg? index)
        (bit-set/set-bit! bits index)
        (recur (inc index))))))

(defn- match-impl
  "Performs a fuzzy text match against a string.
  Returns a two-element vector of [score, matching-indices], or nil if the
  pattern is empty or there is no match. Lower scores are better. The
  matching-indices will be a bit-set containing the character indices where
  either the upper-pattern-chs or the lower-pattern-chs matched the pattern.
  These are expected to be equal-length sequences of non-whitespace code points.
  A bit-set of pattern-break-indices should denote the indices between
  characters where we should expect a run of non-matching characters. The
  from-index is an index into the pattern-chs vectors and can be used to
  control where matching begins in the input."
  [upper-pattern-chs lower-pattern-chs pattern-break-indices ^String string from-index]
  (let [pattern-length (count upper-pattern-chs)
        string-length (.length string)]
    (assert (= pattern-length (count lower-pattern-chs)))
    (when-not (or (zero? pattern-length)
                  (zero? string-length))
      (let [from-index (int from-index)
            best-score-atomic (AtomicInteger. Integer/MAX_VALUE)
            best-matching-indices (bit-set/of-capacity string-length)
            candidate-matching-indices (bit-set/of-capacity string-length)
            search-start-indices (int-array pattern-length 0)

            per-character-indices
            (mapv (fn [^long upper-ch ^long lower-ch]
                    (let [bits (bit-set/of-capacity (.length string))]
                      (assert (not (Character/isWhitespace upper-ch)))
                      (set-matching-index-bits-in-bit-set! bits string upper-ch from-index)
                      (set-matching-index-bits-in-bit-set! bits string lower-ch from-index)
                      bits))
                  upper-pattern-chs
                  lower-pattern-chs)]

        (loop [iteration (int 0)
               tick-index-in-pattern (int 0)]
          (cond
            (= pattern-length tick-index-in-pattern)
            ;; We have a fully formed candidate.
            (let [last-matched-index-in-string (bit-set/last-set-bit candidate-matching-indices)
                  pending-tick-index-in-pattern (dec tick-index-in-pattern)
                  candidate-score (score string from-index candidate-matching-indices)
                  best-score (.get best-score-atomic)]
              (when (< candidate-score best-score)
                ;; We're the new best match.
                ;; Register it, but keep searching for a better match at the
                ;; final position. Yes, the one we found is the closest, but a
                ;; later match after a word boundary could score even better.
                (.set best-score-atomic candidate-score)
                (bit-set/assign! best-matching-indices candidate-matching-indices))
              (bit-set/clear-bit! candidate-matching-indices last-matched-index-in-string)
              (recur (inc iteration)
                     pending-tick-index-in-pattern))

            (or (neg? tick-index-in-pattern)
                (< 1000000 iteration))
            ;; We've checked every permutation, or we've taken too long.
            ;; Return the best result we have.
            (when-not (bit-set/empty? best-matching-indices)
              (let [best-score (.get best-score-atomic)]
                (pair best-score best-matching-indices)))

            :else
            ;; We're still matching.
            ;; When the pattern contains whitespace, we expect a run of
            ;; non-matching characters between the two separated phrases. When
            ;; preparing the pattern, we remove whitespace characters and
            ;; instead put all the break indices in a pattern-break-indices
            ;; bit-set. These are indices between characters in the pattern
            ;; where we expect to find additional non-matching characters. For
            ;; example, "a bc d" becomes "abcd" with break indices at 1 and 3.
            (let [character-indices-in-string (per-character-indices tick-index-in-pattern)
                  search-start-index (aget search-start-indices tick-index-in-pattern)
                  matching-index-in-string (bit-set/next-set-bit character-indices-in-string search-start-index)]
              (if (neg? matching-index-in-string)
                ;; This is a dead branch.
                ;; Backtrack by popping the last matched index from the
                ;; candidate and moving the tick-index back one step.
                (let [pending-tick-index-in-pattern (dec tick-index-in-pattern)
                      last-matched-index-in-string (bit-set/last-set-bit candidate-matching-indices)]
                  (when-not (neg? last-matched-index-in-string)
                    (bit-set/clear-bit! candidate-matching-indices last-matched-index-in-string))
                  (aset search-start-indices tick-index-in-pattern (int 0)) ; Not necessary, but leaving the original value can be confusing while debugging.
                  (recur (inc iteration)
                         pending-tick-index-in-pattern))

                ;; We matched a character.
                (let [pending-tick-index-in-pattern (inc tick-index-in-pattern)
                      next-search-start-index (inc matching-index-in-string)]
                  (bit-set/set-bit! candidate-matching-indices matching-index-in-string)
                  (aset search-start-indices tick-index-in-pattern next-search-start-index)
                  (when (not= pattern-length pending-tick-index-in-pattern)
                    (let [is-pattern-break (bit-set/bit pattern-break-indices pending-tick-index-in-pattern)
                          pending-next-search-start-index (cond-> next-search-start-index is-pattern-break inc)]
                      (aset search-start-indices pending-tick-index-in-pattern pending-next-search-start-index)))
                  (recur (inc iteration)
                         pending-tick-index-in-pattern))))))))))

(defn prepare-pattern
  "Given a pattern string, returns a trimmed and case-agnostic prepared-pattern
  for use with the match functions."
  [^String pattern]
  (let [pattern-break-indices
        (bit-set/of-capacity (.length pattern))

        non-whitespace-chs
        (first
          (stream-reduce!
            (fn [[non-whitespace-chs state :as result] ^long ch]
              (if (Character/isWhitespace ch)
                (case state
                  (:trim :whitespace) result
                  (pair non-whitespace-chs :whitespace))
                (do
                  (case state
                    :whitespace (bit-set/set-bit! pattern-break-indices (count non-whitespace-chs))
                    nil)
                  (pair (conj non-whitespace-chs ch) :character))))
            (pair (vector-of :int) :trim)
            (.codePoints pattern)))

        upper-pattern-chs
        (into (vector-of :int)
              (map ^[int] Character/toUpperCase)
              non-whitespace-chs)

        lower-pattern-chs
        (into (vector-of :int)
              (map ^[int] Character/toLowerCase)
              non-whitespace-chs)]

    [upper-pattern-chs
     lower-pattern-chs
     pattern-break-indices]))

(defn empty-prepared-pattern?
  "Returns true if the supplied prepared-pattern is empty."
  [prepared-pattern]
  (let [upper-pattern-chs (first prepared-pattern)]
    (coll/empty? upper-pattern-chs)))

(defn match
  "Performs a fuzzy text match against a string using the prepared-pattern.
  Returns a two-element vector of [score, matching-indices], or nil if the
  pattern is empty or there is no match. The matching-indices bit-set will
  contain the character indices in the string that matched the pattern in
  sequential order. A lower score represents a better match."
  [prepared-pattern ^String string]
  (let [[upper-pattern-chs lower-pattern-chs pattern-break-indices] prepared-pattern]
    (match-impl upper-pattern-chs lower-pattern-chs pattern-break-indices string 0)))

(defn- apply-filename-bonus
  "Applies a bonus for a match on the filename part of a path."
  [^String path ^long basename-start [^long score matching-indices]]
  (let [basename-end (.lastIndexOf path ".")
        basename-end (if (neg? basename-end) (.length path) basename-end)
        basename-length (- basename-end basename-start)
        ^long basename-streak (loop [matching-index (bit-set/first-set-bit matching-indices)
                                     count 0]
                                (if (and (not= matching-index basename-end)
                                         (= matching-index (+ count basename-start)))
                                  (recur (bit-set/next-set-bit matching-indices (inc matching-index))
                                         (inc count))
                                  count))
        basename-match-adjustment (if (pos? basename-streak)
                                    (quot (* 10 basename-streak) basename-length)
                                    0)]
    (pair (- score basename-match-adjustment 2)
          matching-indices)))

(defn match-path
  "Convenience function for matching against paths. The match is performed
  against the entire path as well as just the file name. We return the
  best-scoring match of the two, or nil if there was no match."
  [prepared-pattern ^String path]
  (let [[upper-pattern-chs lower-pattern-chs pattern-break-indices] prepared-pattern]
    (when-some [path-match (match-impl upper-pattern-chs lower-pattern-chs pattern-break-indices path 0)]
      (if-some [last-slash-index (string/last-index-of path \/)]
        (let [name-index (inc (long last-slash-index))]
          (if-some [name-match
                    (some->> (match-impl upper-pattern-chs lower-pattern-chs pattern-break-indices path name-index)
                             (apply-filename-bonus path name-index))]
            (best-match name-match path-match)
            path-match))
        path-match))))

(defn runs
  "Given a string length and a bit-set of matching-indices inside that string,
  returns a vector of character ranges that should be highlighted or not.
  The ranges are expressed as vectors of [highlight? start-index end-index]. The
  start-index is inclusive, but the end-index is not."
  [^long length matching-indices]
  (loop [prev-matching-index Integer/MIN_VALUE
         matching-index (if matching-indices
                          (bit-set/first-set-bit matching-indices)
                          (int -1))
         runs []]
    (let [run (or (peek runs) [false 0 0])]
      (if (neg? matching-index)
        (if (< ^long (peek run) length)
          (conj runs [false (peek run) length])
          runs)
        (recur matching-index
               (bit-set/next-set-bit matching-indices (inc matching-index))
               (if (= prev-matching-index (dec matching-index))
                 (conj (pop runs) (conj (pop run) (inc matching-index)))
                 (conj (if (< ^long (peek run) matching-index)
                         (conj runs [false (peek run) matching-index])
                         runs)
                       [true matching-index (inc matching-index)])))))))
