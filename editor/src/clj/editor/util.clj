(ns editor.util
  (:import
   (java.util Comparator)))

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))


;; non-lazy implementation of variant of the Alphanum Algorithm:
;; http://www.davekoelle.com/alphanum.html

(defn- alphanum-chunks
  "Returns a vector of groups of consecutive digit or non-digit substrings in
  string. "
  [s]
  (letfn [(make-sb [c]
            (doto (StringBuilder.) (.append c)))
          (digit? [^Character c]
            (when c (Character/isDigit c)))
          (complete-chunk [state ^StringBuilder sb]
            (case state
              :digit (Integer/parseInt (.toString sb))
              :other (.toString sb)))]
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

(def natural-order (reify java.util.Comparator
                     (compare [_ a b]
                       (.compareTo ^java.lang.Comparable (alphanum-chunks a) (alphanum-chunks b)))))

(def natural-order-key alphanum-chunks)
