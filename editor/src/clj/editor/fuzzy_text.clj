(ns editor.fuzzy-text)

;; Sublime Text-style fuzzy text matching. Based on this blog post by Forrest Smith:
;; https://blog.forrestthewoods.com/reverse-engineering-sublime-text-s-fuzzy-match-4cffeed33fdb

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private adjacency-bonus "Bonus for adjacent matches." 5)
(def ^:private separator-bonus "Bonus if match occurs after a separator." 10)
(def ^:private camel-bonus "Bonus if match is uppercase and previous character is lowercase." 10)
(def ^:private leading-letter-penalty "Penalty applied for every letter in str before the first match." -3)
(def ^:private max-leading-letter-penalty "Maximum penalty for leading letters." -9)
(def ^:private unmatched-letter-penalty "Penalty for every letter that doesn't matter." -1)

(defn- char->upper ^Character [ch]
  (Character/toUpperCase ^char ch))

(defn- char->lower ^Character [ch]
  (Character/toLowerCase ^char ch))

(defn- add ^long [^long a ^long b]
  (unchecked-add a b))

(defn match
  "Performs a fuzzy text match against str using the specified pattern.
  Returns a two-element vector of [score, matching-indices], or nil if there
  is no match. The matching-indices vector will contain the character indices
  in str that matched the pattern in sequential order."
  [^String pattern ^String str]
  (let [pattern-length (count pattern)
        str-length (count str)
        best-letter (volatile! nil)
        best-lower (volatile! nil)
        best-letter-idx (volatile! nil)
        best-letter-score (volatile! 0)
        matched-indices (volatile! [])
        score (volatile! 0)]
    (loop [str-idx 0
           pattern-idx 0
           prev-matched? false
           prev-lower? false
           prev-separator? true]
      (if (= str-length str-idx)
        (do (when (some? @best-letter)
              (vswap! score add @best-letter-score)
              (vswap! matched-indices conj @best-letter-idx))
            (when (= pattern-length pattern-idx)
              [@score @matched-indices]))
        (let [pattern-char (when (not= pattern-length pattern-idx) (.charAt pattern pattern-idx))
              pattern-lower (some-> pattern-char char->lower)
              str-char (.charAt str str-idx)
              str-lower (char->lower str-char)
              str-upper (char->upper str-char)
              next-match? (boolean (some-> pattern-char (= str-lower)))
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
                  (let [penalty (max (* str-idx ^long leading-letter-penalty) ^long max-leading-letter-penalty)]
                    (vswap! score add penalty)))
                (let [camel-boundary? (and prev-lower? (= str-upper str-char) (not= str-upper str-lower))
                      new-score (cond-> 0
                                        prev-matched? (+ ^long adjacency-bonus)
                                        prev-separator? (+ ^long separator-bonus)
                                        camel-boundary? (+ ^long camel-bonus))]
                  (when (>= new-score ^long @best-letter-score)
                    (when (nil? @best-letter)
                      (vswap! score add unmatched-letter-penalty))
                    (vreset! best-letter str-char)
                    (vreset! best-lower (char->lower str-char))
                    (vreset! best-letter-idx str-idx)
                    (vreset! best-letter-score new-score)))
                (recur (inc str-idx)
                       (if next-match? (inc pattern-idx) pattern-idx)
                       true
                       (and (= str-lower str-char) (not= str-upper str-lower))
                       (or (= \_ str-char) (= \space str-char))))
            (recur (inc str-idx)
                   pattern-idx
                   false
                   (and (= str-lower str-char) (not= str-upper str-lower))
                   (or (= \_ str-char) (= \space str-char)))))))))
