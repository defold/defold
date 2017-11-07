(ns editor.fuzzy-text
  (:require [clojure.string :as string]))

;; Sublime Text-style fuzzy text matching. Based on this blog post by Forrest Smith:
;; https://blog.forrestthewoods.com/reverse-engineering-sublime-text-s-fuzzy-match-4cffeed33fdb

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private adjacency-bonus "Bonus for adjacent matches." 15)
(def ^:private separator-bonus "Bonus if match occurs after a separator." 15)
(def ^:private camel-bonus "Bonus if match is uppercase and previous character is lowercase." 15)
(def ^:private first-letter-bonus "Bonus if the first letter is matched." 15)
(def ^:private filename-bonus "Bonus for matches to the filename part when using match-path." 20)
(def ^:private leading-letter-penalty "Penalty applied for every letter in str before the first match." -5)
(def ^:private max-leading-letter-penalty "Maximum penalty for leading letters." -15)
(def ^:private unmatched-letter-penalty "Penalty for every letter that doesn't matter." -1)
(def ^:private separator-chars #{\space \/ \_ \-})

(defn- char-blank? [ch]
  (Character/isWhitespace ^char ch))

(defn- char->upper ^Character [ch]
  (Character/toUpperCase ^char ch))

(defn- char->lower ^Character [ch]
  (Character/toLowerCase ^char ch))

(defn- add ^long [^long a ^long b]
  (unchecked-add a b))

(defn- sub ^long [^long a ^long b]
  (unchecked-subtract a b))

(defn- mul ^long [^long a ^long b]
  (unchecked-multiply a b))

(defn- match-impl [^String pattern ^String str ^long start-index ^long offset]
  (let [pattern-length (count pattern)
        str-length (count str)
        best-letter (volatile! nil)
        best-lower (volatile! nil)
        best-letter-idx (volatile! nil)
        best-letter-score (volatile! 0)
        matched-indices (volatile! [])
        score (volatile! 0)]
    (loop [str-idx (+ start-index offset)
           pattern-idx 0
           prev-matched-idx nil
           prev-lower? false
           prev-separator? true]
      (if (= str-length str-idx)
        (do (when (some? @best-letter)
              (vswap! score add @best-letter-score)
              (vswap! matched-indices conj @best-letter-idx))
            (let [final-matched-indices @matched-indices]
              (when (and (= pattern-length pattern-idx) (seq final-matched-indices))
                [@score final-matched-indices])))
        (let [pattern-char (when (not= pattern-length pattern-idx) (.charAt pattern pattern-idx))]
          (if (and pattern-char (char-blank? pattern-char))
            (recur str-idx
                   (inc pattern-idx)
                   prev-matched-idx
                   prev-lower?
                   prev-separator?)
            (let [pattern-lower (some-> pattern-char char->lower)
                  str-char (.charAt str str-idx)
                  str-lower (char->lower str-char)
                  str-upper (char->upper str-char)
                  next-match? (and pattern-char (= str-lower pattern-lower))
                  rematch? (boolean (some-> @best-letter (= str-lower)))
                  advanced? (and next-match? @best-letter)
                  pattern-repeat? (and @best-letter pattern-char (= pattern-lower @best-lower))]
              (when (or advanced? pattern-repeat?)
                (vswap! score add @best-letter-score)
                (vswap! matched-indices conj @best-letter-idx)
                (vreset! best-letter nil)
                (vreset! best-lower nil)
                (vreset! best-letter-idx nil)
                (vreset! best-letter-score 0))
              (if (or next-match? rematch?)
                (do (when (zero? pattern-idx)
                      ;; NOTE: Penalties are negative. Use max to find the lowest penalty.
                      (let [penalty (max (mul (sub str-idx start-index) leading-letter-penalty) ^long max-leading-letter-penalty)]
                        (vswap! score add penalty)))
                    (let [camel-boundary? (and prev-lower? (= str-upper str-char) (not= str-upper str-lower))
                          first-letter? (= start-index str-idx)
                          new-score (cond-> 0
                                            (= prev-matched-idx (dec str-idx)) (add adjacency-bonus)
                                            prev-separator? (add separator-bonus)
                                            camel-boundary? (add camel-bonus)
                                            first-letter? (add first-letter-bonus))]
                      (when (>= new-score ^long @best-letter-score)
                        (when (nil? @best-letter)
                          (vswap! score add unmatched-letter-penalty))
                        (vreset! best-letter str-char)
                        (vreset! best-lower (char->lower str-char))
                        (vreset! best-letter-idx str-idx)
                        (vreset! best-letter-score new-score)))
                    (recur (inc str-idx)
                           (if next-match? (inc pattern-idx) pattern-idx)
                           (when-not rematch? str-idx)
                           (and (= str-lower str-char) (not= str-upper str-lower))
                           (contains? separator-chars str-char)))
                (recur (inc str-idx)
                       pattern-idx
                       prev-matched-idx
                       (and (= str-lower str-char) (not= str-upper str-lower))
                       (contains? separator-chars str-char))))))))))

(defn match
  "Performs a fuzzy text match against str using the specified pattern.
  Returns a two-element vector of [score, matching-indices], or nil if there
  is no match. The matching-indices vector will contain the character indices
  in str that matched the pattern in sequential order."
  ([^String pattern ^String str]
   (match pattern str 0))
  ([^String pattern ^String str ^long start-index]
   (loop [[best-score :as best-match] nil
          offset 0]
     (if-some [[score matching-indices :as match] (match-impl pattern str start-index offset)]
       (recur (if (or (nil? best-score) (>= ^long score ^long best-score)) match best-match)
              (inc (sub (first matching-indices) start-index)))
       best-match))))

(defn- add-filename-bonus [[^long score matched-indices]]
  [(+ score ^long filename-bonus) matched-indices])

(defn match-path
  "Convenience function for matching against paths. The match function is
  called for the entire path as well as just the file name. We return the
  best-scoring match of the two, or nil if there was no match."
  [^String pattern ^String path]
  (when-some [path-match (match pattern path)]
    (if-some [last-slash-index (string/last-index-of path \/)]
      (let [name-index (inc ^long last-slash-index)]
        (if-some [name-match (some-> (match pattern path name-index) add-filename-bonus)]
          (max-key first name-match path-match)
          path-match))
      path-match)))

(defn runs
  "Given a string length and a sequence of matching indices inside that string,
  returns a vector of character ranges that should be highlighted or not.
  The ranges are expressed as vectors of [highlight? start-index end-index]. The
  start-index is inclusive, but the end-index is not."
  [^long length matching-indices]
  (loop [prev-matching-index nil
         matching-indices matching-indices
         runs []]
    (let [^long matching-index (first matching-indices)
          run (or (peek runs) [false 0 0])]
      (if (nil? matching-index)
        (if (< ^long (peek run) length)
          (conj runs [false (peek run) length])
          runs)
        (recur matching-index
               (next matching-indices)
               (if (= prev-matching-index (dec matching-index))
                 (conj (pop runs) (conj (pop run) (inc matching-index)))
                 (conj (if (< ^long (peek run) matching-index)
                         (conj runs [false (peek run) matching-index])
                         runs)
                       [true matching-index (inc matching-index)])))))))
