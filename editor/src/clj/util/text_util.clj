;; Copyright 2020-2025 The Defold Foundation
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

(ns util.text-util
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [util.coll :refer [pair]])
  (:import [clojure.lang IReduceInit]
           [java.io BufferedReader Reader StringReader]
           [java.util.regex MatchResult Pattern]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn text-char?
  "Returns true if the supplied character is textual. This can
  be used to determine if we're dealing with binary data."
  [^Character char]
  (or (Character/isWhitespace char)
      (not (Character/isISOControl char))))

(defn guess-line-endings
  "Reads character from the supplied fresh reader until it can determine the
  type of line ending used in the text. If reader is nil, does not contain line
  endings, or the data looks like it might be a binary file, returns nil. Will
  close the reader when done."
  [^Reader reader]
  (when (some? reader)
    (with-open [_ reader]
      (loop [ch (.read reader)
             prev-ch nil
             lf-count 0
             crlf-count 0]
        (cond
          ; If we've reached the end or seen enough, return result.
          (or (= -1 ch) (< 250 (max lf-count crlf-count)))
          (cond
            (= 0 lf-count crlf-count) nil
            (> lf-count crlf-count) :lf
            :else :crlf)

          (= 0x0A ch) ; LF character.
          (if (= 0x0D prev-ch) ; Previous was CR character.
            (recur (.read reader) ch lf-count (inc crlf-count))
            (recur (.read reader) ch (inc lf-count) crlf-count))

          (not (text-char? (char ch))) ; This looks like binary! Return nil.
          nil

          :else
          (recur (.read reader) ch lf-count crlf-count))))))

(defn guess-line-separator
  "Tries to guess the type of line endings used in the supplied input string and
  returns a suitable line separator string. If the type of line endings in use
  cannot be determined, defaults to a single newline character."
  ^String [^String text]
  (case (some-> text .getBytes io/reader guess-line-endings)
    :crlf "\r\n"
    "\n"))

(defn scan-line-endings
  "Reads character from the supplied fresh reader until it reaches the end of
  the file, finds mixed line endings, or detects that the file is binary. For a
  text file with consistent line endings, it will read the entire file. Returns
  the type of line ending used in the file, or :mixed if the file mixes line
  endings. If reader is nil, does not contain line endings, or the data looks
  like it might be a binary file, returns nil. Will close the reader when done."
  [^Reader reader]
  (when (some? reader)
    (with-open [_ reader]
      (loop [ch (.read reader)
             prev-ch nil
             line-ending nil]
        (cond
          (= -1 ch) ; We've reached the end, return result.
          line-ending

          (= 0x0A ch) ; LF character.
          (cond
            (= 0x0D prev-ch) ; Previous was CR character.
            (if (= :lf line-ending)
              :mixed
              (recur (.read reader) ch :crlf))

            (= :crlf line-ending)
            :mixed

            :else
            (recur (.read reader) ch :lf))

          (not (text-char? (char ch))) ; This looks like binary! Return nil.
          nil

          :else
          (recur (.read reader) ch line-ending))))))

(defn crlf->lf
  "Converts all CRLF line endings in the input string to LF.
  Broken line endings such as CRCRLF will be converted into a single LF.
  Rouge CR characters will be removed.
  Returns nil if the input is nil."
  [^String text]
  (some-> text (string/replace #"\r" "")))

(defn lf->crlf
  "Converts all LF line endings in the input string to CRLF.
  Broken line endings such as CRCRLF will be converted into a single CRLF.
  Rouge CR characters will be removed.
  Returns nil if the input is nil."
  [^String text]
  (some-> text (string/replace #"\r" "") (string/replace #"\n" "\r\n")))

(defn binary?
  "Tries to guess whether some readable thing has binary content. Looks at the
  first `chars-to-check` number of chars from readable and if more than
  `binary-chars-threshold` of them are not `text-char?` then consider the
  content binary."
  ([readable]
   (binary? readable nil))
  ([readable {:keys [chars-to-check binary-chars-threshold]
              :or   {chars-to-check 1000
                     binary-chars-threshold 0.01}}]
   (with-open [rdr (io/reader readable)]
     (let [cbuf (char-array chars-to-check)
           nread (.read rdr cbuf 0 chars-to-check)
           limit (long (* (double binary-chars-threshold) nread))]
       (loop [i 0
              binary-chars 0]
         (if (< i nread)
           (if (< limit binary-chars)
             true
             (recur (inc i) (if (text-char? (aget cbuf i)) binary-chars (inc binary-chars))))
           false))))))

(defn- consume-separator [^String str ^long start-index]
  (loop [quote-char-count 0
         index start-index]
    (if (= index (count str))
      [index \,]
      (let [char (.charAt str index)]
        (cond
          (= \" char)
          (recur (inc quote-char-count) (inc index))

          (or (= \, char) (Character/isWhitespace char))
          (recur quote-char-count (inc index))

          :else
          (let [quoted? (odd? quote-char-count)
                stop-char (if quoted? \" \,)]
            [index stop-char]))))))

(defn parse-comma-separated-string
  "Parses a string of comma-separated, optionally quoted strings into a vector of unquoted strings."
  [^String str]
  (let [[^long start-index stop-char] (consume-separator str 0)]
    (loop [substrings (transient [])
           builder (StringBuilder.)
           index start-index
           ^char stop-char stop-char]
      (if (= index (count str))
        (persistent!
          (if (zero? (.length builder))
            substrings
            (conj! substrings (.toString builder))))
        (let [char (.charAt str index)]
          (if (= char stop-char)
            (let [[next-index next-stop-char] (consume-separator str (inc index))]
              (recur (conj! substrings (.toString builder))
                     (StringBuilder.)
                     next-index
                     next-stop-char))
            (recur substrings
                   (.append builder char)
                   (inc index)
                   stop-char)))))))

(defn join-comma-separated-string
  ^String [strings]
  (let [inner-string (string/join "\", \"" (remove empty? strings))]
    (if (empty? inner-string)
      inner-string
      (str "\"" inner-string "\""))))

(defn character-count
  "Returns the number of occurrences of the specified char-or-code-point in the string."
  ^long [^String str char-or-code-point]
  (let [length (.length str)
        code-point (int char-or-code-point)]
    (loop [index (int 0)
           found-count 0]
      (if (== length index)
        found-count
        (recur (inc index)
               (if (== code-point (.codePointAt str index))
                 (inc found-count)
                 found-count))))))

(defn- line-info-coll [make-buffered-reader]
  (reify IReduceInit
    (reduce [_ rf init]
      (with-open [^BufferedReader reader (make-buffered-reader)]
        (loop [ret init
               row 0
               caret-position 0]
          (if-some [line (.readLine reader)]
            (let [ret (rf ret {:line line
                               :row row
                               :caret-position caret-position})]
              (if (reduced? ret)
                @ret
                (recur ret
                       (inc row)
                       (+ caret-position (count line)))))
            ret))))))

(defn- readable->line-infos [readable]
  (line-info-coll #(io/reader readable)))

(defn- text->line-infos [^String text]
  (line-info-coll #(BufferedReader. (StringReader. text))))

(defn- make-text-match
  ([^String text ^MatchResult match-result]
   (make-text-match text match-result 0 0))
  ([^String text ^MatchResult match-result ^long row ^long row-start-caret-position]
   (let [start-col (.start match-result)
         end-col (.end match-result)]
     {:match-type :match-type-text
      :text text
      :row row
      :start-col start-col
      :end-col end-col
      :caret-position (+ row-start-caret-position start-col)})))

(defn- line-infos->text-matches [line-infos ^Pattern re-pattern]
  (persistent!
    (reduce
      (fn [matches line-info]
        (let [line (:line line-info)
              matcher (re-matcher re-pattern line)]
          (loop [matches matches]
            (if-not (.find matcher)
              matches
              (recur (conj! matches
                            (make-text-match line matcher (:row line-info) (:caret-position line-info))))))))
      (transient [])
      line-infos)))

(defn lines->text-matches [lines ^Pattern re-pattern]
  (persistent!
    (second
      (reduce-kv
        (fn [[^long line-start-pos matches] ^long row ^String line]
          (let [next-line-start-pos (+ line-start-pos (count line))
                matcher (re-matcher re-pattern line)]
            (loop [matches matches]
              (if-not (.find matcher)
                (pair next-line-start-pos matches)
                (recur (conj! matches
                              (make-text-match line matcher row line-start-pos)))))))
        (pair 0 (transient []))
        lines))))

(defn readable->text-matches [readable ^Pattern re-pattern]
  (line-infos->text-matches (readable->line-infos readable) re-pattern))

(defn text->text-matches [^String text ^Pattern re-pattern]
  (line-infos->text-matches (text->line-infos text) re-pattern))

(defn string->text-match [^String string ^Pattern re-pattern]
  (let [matcher (re-matcher re-pattern string)]
    (when (.find matcher)
      (make-text-match string matcher))))

(defn search-string-numeric?
  "Returns true if the supplied search string might match a numeric value."
  [^String string-with-wildcards]
  (and (pos? (.length string-with-wildcards))
       (some? (re-matches #"^-?[\d*]*\.?[\d*]*$" string-with-wildcards))))

(defn search-string->re-pattern
  "Convert a search string that may contain wildcards and special characters to
  a Java regex Pattern. The case-sensitivity can be either :case-sensitive or
  :case-insensitive."
  ^Pattern [^String string-with-wildcards case-sensitivity]
  (let [re-string (->> (string/split string-with-wildcards #"\*")
                       (map #(Pattern/quote %))
                       (string/join ".*"))]
    (re-pattern
      (case case-sensitivity
        :case-sensitive re-string
        :case-insensitive (str "(?i)" re-string)))))

(defn includes-re-pattern?
  "Returns true if the re-pattern matches any part of the text."
  [^String text ^Pattern re-pattern]
  (let [matcher (re-matcher re-pattern text)]
    (.find matcher)))

(defn includes-ignore-case?
  "Like clojure.string/includes?, but case-insensitive"
  [^String str ^String sub]
  (let [sub-length (.length sub)]
    (if (zero? sub-length)
      true
      (let [str-length (.length str)]
        (loop [i 0]
          (cond
            (= i str-length) false
            (.regionMatches str true i sub 0 sub-length) true
            :else (recur (inc i))))))))
