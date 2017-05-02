(ns editor.util
  (:require [clojure.string :as string]
            [clojure.java.io :as io])
  (:import
   [java.io File]
   [java.nio.file Path Paths Files attribute.FileAttribute CopyOption StandardCopyOption]
   [org.apache.commons.io FileUtils]))

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

(defn- find-entries [^File src-dir ^File tgt-dir]
  (let [base (.getAbsolutePath src-dir)
        base-count (inc (count base))]
    (mapv (fn [^File src-entry]
            (let [rel-path (subs (.getAbsolutePath src-entry) base-count)
                  tgt-entry (File. tgt-dir rel-path)]
              [src-entry tgt-entry]))
          (.listFiles src-dir))))

(declare move-entry!)

(defn move-directory! [^File src ^File tgt]
  (try
    (.mkdirs tgt)
    (catch SecurityException _))
  (let [moved-file-pairs (into []
                               (mapcat (fn [[child-src child-tgt]]
                                         (move-entry! child-src child-tgt)))
                               (find-entries src tgt))]
    (when (empty? (.listFiles src))
      (try
        (when-not (.canWrite src)
          (.setWritable src true))
        (FileUtils/deleteQuietly src)
        (catch SecurityException _)))
    moved-file-pairs))

(defn move-file! [^File src ^File tgt]
  (if (try
        (if (.exists tgt)
          (when-not (.canWrite tgt)
            (.setWritable tgt true))
          (io/make-parents tgt))
        (FileUtils/deleteQuietly tgt)
        (.renameTo src tgt)
        (catch SecurityException _))
    [[src tgt]]
    []))

(defn move-entry!
  "Moves a file system entry to the specified target location. Any existing
  files at the target location will be overwritten. If it does not already
  exist, the path leading up to the target location will be created. Returns
  a sequence of [source, destination] File pairs that were successfully moved."
  [^File src ^File tgt]
  (if (.isDirectory src)
    (move-directory! src tgt)
    (move-file! src tgt)))



(defn is-mac-os? []
  (= "Mac OS X" (System/getProperty "os.name")))

(defn positions [pred coll]
  (keep-indexed (fn [idx x]
                  (when (pred x)
                    idx))
                coll))

(defn delete-on-exit!
  [^File file]
  (if (.isDirectory file)
    (.. Runtime getRuntime (addShutdownHook (Thread. #(FileUtils/deleteQuietly file))))
    (.deleteOnExit file)))
