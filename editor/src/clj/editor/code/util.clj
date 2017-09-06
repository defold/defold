(ns editor.code.util
  (:import (clojure.lang MapEntry)
           (java.util.regex Matcher Pattern)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn pair [a b]
  "Returns a two-element collection that implements IPersistentVector."
  (MapEntry/create a b))

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
