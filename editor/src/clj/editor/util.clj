(ns editor.util
  (:require [clojure.string :as string])
  (:import [java.util Locale]
           [com.defold.editor Platform]))

(set! *warn-on-reflection* true)

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))


;; See http://mattryall.net/blog/2009/02/the-infamous-turkish-locale-bug

(defn lower-case*
  [^CharSequence s]
  "Like clojure.string/lower-case but using root locale."
  (.. s toString (toLowerCase Locale/ROOT)))

(defn upper-case*
  [^CharSequence s]
  "Like clojure.string/upper-case but using root locale."
  (.. s toString (toUpperCase Locale/ROOT)))

(defn capitalize*
  "Like clojure.string/capitalize but using root locale."
  ^String [^CharSequence s]
  (let [s (.toString s)]
    (if (< (count s) 2)
      (.toUpperCase s Locale/ROOT)
      (str (.toUpperCase (subs s 0 1) Locale/ROOT)
           (.toLowerCase (subs s 1) Locale/ROOT)))))

(defn format*
  "Like clojure.core/format but using root locale."
  ^String [fmt & args]
  (String/format Locale/ROOT fmt (to-array args)))


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
              :digit (bigint (.toString sb))
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

(defn os-raw
  "Returns :win32, :darwin or :linux"
  []
  (keyword (.. Platform getHostPlatform getOs)))

(def os (memoize os-raw))

(defn is-mac-os? []
  (= (os) :darwin))

(defn positions [pred coll]
  (keep-indexed (fn [idx x]
                  (when (pred x)
                    idx))
                coll))
