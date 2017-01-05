(ns editor.util
  (:require [clojure.string :as string])
  (:import
    [java.io File]))

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))


;; non-lazy implementation of variant of the Alphanum Algorithm:
;; http://www.davekoelle.com/alphanum.html

(defn- alphanum-chunks
  "Returns a vector of groups of consecutive digit or non-digit substrings in
  string. The strings are converted to lowercase."
  [s]
  (letfn [(make-sb [c]
            (doto (StringBuilder.) (.append c)))
          (digit? [^Character c]
            (when c (Character/isDigit c)))
          (complete-chunk [state ^StringBuilder sb]
            (case state
              :digit (Integer/parseInt (.toString sb))
              :other (string/lower-case (.toString sb))))]
    (loop [[c & cs]          (seq s)
           state             (if (digit? c) :digit :other)
           ^StringBuilder sb (StringBuilder.)
           ret []]
      (if c
        (case state
          :digit (if (digit? c)
                   (recur cs :digit (.append sb c) ret)
                   (recur cs :other (make-sb c) (conj ret (complete-chunk state sb))))
          :other (if-not (digit? c)
                   (recur cs :other (.append sb c) ret)
                   (recur cs :digit (make-sb c) (conj ret (complete-chunk state sb)))))
        (cond-> ret (pos? (.length sb)) (conj (complete-chunk state sb)))))))

(defn- start-with-string [v]
  (let [v0 (first v)]
    (if (not (string? v0))
      (assoc v 0 (str v0))
      v)))

(defn- pad-nil [v l]
  (let [pad (- l (count v))]
    (into v (repeat pad nil))))

(def natural-order (reify java.util.Comparator
                     (compare [_ a b]
                       (if (and a b)
                         (let [ancs (map (comp start-with-string alphanum-chunks) [a b])
                               l (reduce max (map count ancs))
                               [a' b'] (map #(pad-nil % l) ancs)]
                           (compare a' b'))
                         (compare a b)))))

(def natural-order-key alphanum-chunks)

(defn to-folder ^File [^File file]
  (if (.isFile file) (.getParentFile file) file))
