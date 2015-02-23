(ns dynamo.messages
  (:import [java.util ResourceBundle])
  (:require [clojure.java.io :as io]))

(defn bundle-map
  [^ResourceBundle bundle]
  (let [ks (enumeration-seq (.getKeys bundle))]
    (zipmap ks (map #(.getString bundle %) ks))))

(defn load-bundle-in-namespace
  [ns-sym bundle]
  (create-ns ns-sym)
  (doseq [[k v] (bundle-map bundle)]
    (intern ns-sym (with-meta (symbol k) {:bundle bundle}) v)))

(defn resource-bundle
  [path]
  (ResourceBundle/getBundle path))

(load-bundle-in-namespace 'dynamo.messages (resource-bundle "messages"))
