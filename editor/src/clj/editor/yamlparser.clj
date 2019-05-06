(ns editor.yamlparser
  (:refer-clojure :exclude [load])
  (:import [java.io Reader]
           [java.util List Map]
           [org.snakeyaml.engine.v1.api Load LoadSettingsBuilder])
  (:require [clojure.java.io :as io]
            [clojure.walk :refer [walk]]))

(defn- translate
  [x]
  (let [wlk (partial walk translate identity)
        f (condp instance? x
            ;; Turn maps into clojure maps and change keys to keywords, recurse
            Map #(wlk (reduce (fn [m e] (assoc m (keyword (key e)) (val e))) {} %))
            ;; Turn lists into clojure vectors, recurse
            List #(wlk (into [] %))
            ;; Other values we leave as is.
            identity)]
    (f x)))

(defn- new-loader
  ^Load []
  (-> (LoadSettingsBuilder.)
      (.build)
      (Load.)))

(defn load
  "Load a Yaml structure.

  `source` must be an instance of `java.lang.String` or `java.io.Reader`.

  The result is a nested collection of maps and vectors where the keys in maps
  have been translated from strings to keywords."
  [source]
  (->
    (condp instance? source
      String (.loadFromString (new-loader) source)
      Reader (.loadFromReader (new-loader) source)
      (throw (IllegalArgumentException. "Can only load from a String or Reader.")))
    (translate)))
