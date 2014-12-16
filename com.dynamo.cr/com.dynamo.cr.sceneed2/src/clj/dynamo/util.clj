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
