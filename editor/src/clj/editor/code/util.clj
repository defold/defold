(ns editor.code.util
  (:require [clojure.string :as string])
  (:import (clojure.lang MapEntry)
           (java.util.regex Matcher Pattern)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn pair
  "Returns a two-element collection that implements IPersistentVector."
  [a b]
  (MapEntry/create a b))

(defn split-lines
  "Splits s on \\n or \\r\\n. Contrary to string/split-lines, keeps trailing newlines."
  [text]
  (string/split text #"\r?\n" -1))

(defn re-matcher-from
  "Returns an instance of java.util.regex.Matcher that starts at an offset."
  ^Matcher [re s start]
  (let [whole-string-matcher (re-matcher re s)]
    (if (> ^long start 0)
      (.region whole-string-matcher start (count s))
      whole-string-matcher)))

(defn re-match-result-seq
  "Returns a lazy sequence of successive MatchResults of the pattern in the
  specified string, using java.util.regex.Matcher.find(), with an optional
  start offset into the string."
  ([^Pattern re s]
   (re-match-result-seq re s 0))
  ([^Pattern re s ^long start]
   (let [m (re-matcher-from re s start)]
     ((fn step []
        (when (. m (find))
          (cons (.toMatchResult m) (lazy-seq (step)))))))))
