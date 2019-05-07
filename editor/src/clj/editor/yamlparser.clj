(ns editor.yamlparser
  (:refer-clojure :exclude [load])
  (:import [java.io Reader]
           [java.util List Map]
           [org.snakeyaml.engine.v1.api Load LoadSettingsBuilder])
  (:require [clojure.java.io :as io]
            [clojure.walk :refer [walk]]))

(defn- translate
  [x key-transform-fn]
  (let [wlk (partial walk #(translate % key-transform-fn) identity)
        f (condp instance? x
            ;; Turn maps into clojure maps and change keys to keywords, recurse
            Map #(wlk (reduce (fn [m [k v]] (assoc m (key-transform-fn k) v)) {} %))
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

  The result is a nested collection of maps and vectors. You may optionally pass
  in a function to transform the keys of any maps in the result. For example,
  passing the keyword function will transform all the map keys to keywords."
  ([source]
   (load source identity))
  ([source key-transform-fn]
   (-> (condp instance? source
         String (.loadFromString (new-loader) source)
         Reader (.loadFromReader (new-loader) source)
         (throw (IllegalArgumentException. "Can only load from a String or Reader.")))
       (translate key-transform-fn))))
