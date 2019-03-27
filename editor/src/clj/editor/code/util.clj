(ns editor.code.util
  (:require [clojure.string :as string])
  (:import [clojure.lang MapEntry]
           [java.util ArrayList Collections Comparator List]
           [java.util.regex Matcher Pattern]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- comparisons-syntax [exp & rest-exps]
  (if (empty? rest-exps)
    exp
    (let [comparison-sym (gensym "comparison")]
      `(let [~comparison-sym ~exp]
         (if (zero? ~comparison-sym)
           ~(apply comparisons-syntax rest-exps)
           ~comparison-sym)))))

(defmacro comparisons [& exps]
  (apply comparisons-syntax exps))

(defn- ->insert-index
  ^long [^long binary-search-result]
  ;; Collections/binarySearch returns a non-negative value if an exact match is
  ;; found. Otherwise it returns (-(insertion point) - 1). The insertion point
  ;; is defined as the point at which the key would be inserted into the list:
  ;; the index of the first element greater than the key, or list.size() if all
  ;; elements in the list are less than the specified key.
  (if (neg? binary-search-result)
    (Math/abs (inc binary-search-result))
    (inc binary-search-result)))

(defn find-insert-index
  "Finds the insertion index in an ordered collection. If the collection is not
  ordered, the result is undefined. New items will be inserted after exact
  matches, or between two non-exact matches that each compare differently to
  item using the supplied comparator."
  ([coll item]
   (find-insert-index coll item compare))
  ([^List coll item ^Comparator comparator]
   (let [search-result (Collections/binarySearch coll item comparator)]
     (->insert-index search-result))))

(defn insert
  "Inserts an item at the specified position in a vector. Shifts the item
  currently at that position (if any) and any subsequent items to the right
  (adds one to their indices). Returns the new vector."
  [coll index item]
  (into (subvec coll 0 index)
        (cons item
              (subvec coll index))))

(defn insert-sort
  "Inserts an item into an ordered vector. If the collection is not ordered, the
  result is undefined. The insert index will be determined by comparing items.
  New items will be inserted after exact matches, or between two non-exact
  matches that each compare differently to item using the supplied comparator.
  Returns the new vector."
  ([coll item]
   (insert-sort coll item compare))
  ([coll item comparator]
   (insert coll (find-insert-index coll item comparator) item)))

(defn last-index-where
  "Returns the index of the last element in coll where pred returns true,
  or nil if there was no matching element. The coll must support addressing
  using nth."
  [pred coll]
  (loop [index (dec (count coll))]
    (when-not (neg? index)
      (if (pred (nth coll index))
        index
        (recur (dec index))))))

(defn pair
  "Returns a two-element collection that implements IPersistentVector."
  [a b]
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

(defn split-lines
  "Splits s on \\n or \\r\\n. Contrary to string/split-lines, keeps trailing
  newline at the end of a text."
  [^String text]
  ;; This is basically java code, for speed. This function is used a lot when
  ;; loading projects so it needs to be fast. String.split with regex is very
  ;; slow in comparison. I read that modern jvms are good at optimizing out the
  ;; .charAt call so this *should* be like a loop over a char array...
  (let [arr (ArrayList. 8192)
        len (.length text)
        sb (StringBuilder.)]
    (loop [i 0
           last-index 0
           last-char \x]
      (if (< i len)
        (let [c (.charAt text i)
              i (inc i)]
          (case c

            \newline
            (if-not (= last-char \return)
              (do
                (.add arr (.toString sb))
                (.setLength sb 0)
                (recur i i c))
              (recur i i c))

            \return
            (do
              (.add arr (.toString sb))
              (.setLength sb 0)
              (recur i i c))

            ;; default
            (do
              (.append sb c)
              (recur i last-index c))))
        ;; else branch
        (do
          (.add arr (.toString sb))
          (into [] arr))))))

