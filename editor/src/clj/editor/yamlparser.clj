;; Copyright 2020 The Defold Foundation
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.yamlparser
  (:refer-clojure :exclude [load])
  (:require [clojure.walk :refer [walk]])
  (:import [java.io Reader]
           [java.util List Map]
           [org.snakeyaml.engine.v1.api Load LoadSettingsBuilder]))

(defn- translate
  [x key-transform-fn]
  (let [wlk (fn [x] (walk #(translate % key-transform-fn) identity x))]
    (condp instance? x
      ;; Turn maps into clojure maps and change keys to keywords, recurse
      Map (wlk (reduce (fn [m [k v]] (assoc m (key-transform-fn k) v)) {} x))
      ;; Turn lists into clojure vectors, recurse
      List (wlk (into [] x))
      ;; Other values we leave as is.
      x)))

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
