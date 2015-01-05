(ns dynamo.util
  (:require [clojure.string :as str]))

(defn map-keys
  [f m]
  (zipmap
    (map f (keys m))
    (vals m)))

(defn map-vals
  [f m]
  (zipmap
    (keys m)
    (map f (vals m))))

(defn map-diff
  [m1 m2 sub-fn]
  (reduce-kv
    (fn [result k v]
      (if (contains? result k)
        (let [ov (get result k)]
          (let [nv (sub-fn ov v)]
            (if (empty? nv)
              (dissoc result k)
              (assoc result k nv))))
        result))
    m1
    m2))

(defn parse-number
  "Reads a number from a string. Returns nil if not a number."
  [s]
  (when (re-find #"^-?\d+\.?\d*$" s)
    (read-string (str/replace s #"^(-?)0*(\d+\.?\d*)$" "$1$2"))))

(defn parse-int
  "Reads an integer from a string. Returns nil if not an integer."
  [s]
  (when (re-find #"^-?\d+\.?0*$" s)
    (read-string (str/replace s #"^(-?)0*(\d+)\.?0*$" "$1$2"))))

(defmacro monitored-task
 [mon nm size & body]
 `(let [m# ~mon]
    (.beginTask m# ~nm ~size)
    (let [res# (do ~@body)]
      (.done m#)
      res#)))

(defmacro monitored-work
  [mon subtask & body]
  `(let [m# ~mon]
     (.subTask m# ~subtask)
     (let [res# (do ~@body)]
       (.worked m# 1)
       res#)))

(defmacro doseq-monitored [monitor task-name bindings & body]
  (assert (= 2 (count bindings)) "Monitored doseq only allows one binding")
  `(let [items# ~(second bindings)]
     (.beginTask ~monitor ~task-name (count items#))
     (loop [continue# true
            items#     items#]
       (when continue#
         (when (seq items#)
           (let [~(first bindings) (first items#)]
             ~@body)
           (.worked ~monitor 1)
           (recur (.isCanceled ~monitor) (rest items#)))))
     (.done ~monitor)))
