;; Copyright 2020-2022 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

(ns editor.yaml
  (:refer-clojure :exclude [load])
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.util :as util])
  (:import [clojure.lang Keyword]
           [com.defold.snakeyaml OpenRepresenter]
           [java.io Reader]
           [java.util List Map LinkedHashMap Collection]
           [java.util.function BiConsumer]
           [org.snakeyaml.engine.v1.api Load LoadSettingsBuilder Dump DumpSettingsBuilder RepresentToNode]
           [org.snakeyaml.engine.v1.common ScalarStyle]
           [org.snakeyaml.engine.v1.exceptions Mark MarkedYamlEngineException]
           [org.snakeyaml.engine.v1.nodes ScalarNode Tag]))

(defn- translate-on [key-fn]
  (fn java->clj [x]
    (condp instance? x
      Map (let [acc (volatile! (transient {}))]
            (.forEach ^Map x
                      (reify BiConsumer
                        (accept [_ k v]
                          (vswap! acc assoc! (key-fn (java->clj k)) (java->clj v)))))
            (persistent! @acc))
      List (into [] (map java->clj) x)
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
   ((translate-on key-transform-fn)
    (condp instance? source
      String (.loadFromString (new-loader) source)
      Reader (.loadFromReader (new-loader) source)
      (throw (IllegalArgumentException. "Can only load from a String or Reader."))))))

(defmacro with-error-translation
  "Perform body, convert thrown parse exceptions to error values

  These exceptions might be thrown when e.g. calling [[load]]"
  [node-id label resource & body]
  `(try
     ~@body
     (catch MarkedYamlEngineException ex#
       (let [mark# ^Mark (.get (.getProblemMark ex#))
             row# (.getLine mark#)
             col# (.getColumn mark#)
             cursor-range# (data/Cursor->CursorRange (data/->Cursor row# col#))
             message# (string/trim (.getMessage ex#))
             node-id# ~node-id]
         (g/->error node-id# ~label :fatal nil message# {:type :invalid-content
                                                         :resource ~resource
                                                         :cursor-range cursor-range#})))))

(def ^:private keyword-representer
  (reify RepresentToNode
    (representData [_ kw]
      (ScalarNode. Tag/STR (name kw) ScalarStyle/PLAIN))))

(defn- new-dumper
  ^Dump
  [{:keys [indent]
    :or {indent 2}}]
  (let [dump-settings (-> (DumpSettingsBuilder.)
                          (.setIndent indent)
                          (.setSplitLines false)
                          (.build))
        representer (OpenRepresenter. dump-settings)]
    (doto (.getRepresenters representer)
      (.put Keyword keyword-representer))
    (Dump. dump-settings representer)))

(defn- make-ordered
  ([value pattern]
   (make-ordered value
                 (into {}
                       (map-indexed
                         (fn [i k-or-vec]
                           [(if (vector? k-or-vec) (k-or-vec 0) k-or-vec) i]))
                       pattern)
                 (into {} (filter vector?) pattern)))
  ([value order key->pattern]
   (condp instance? value
     Map (->> value
              (sort-by key (util/comparator-chain
                             (util/comparator-on #(order % ##Inf))
                             compare))
              (reduce (fn [^LinkedHashMap map [k v]]
                        (doto map (.put k (if-let [p (key->pattern k)]
                                            (make-ordered v p)
                                            v))))
                      (LinkedHashMap.)))
     Collection (mapv #(make-ordered % order key->pattern) value)
     value)))

(defn ^{:arglists '([x & {:keys [order-pattern indent]}])} dump
  "Convert data structure to YAML string, optionally setting map key order

  Supported opts:

    :order-pattern    defines key order in a nested data structure, a vector
                      of either:
                      * key in its order
                      * 2-element tuple of key and another order pattern for
                        maps found on that key
    :indent           yaml formatting indent, default 2"
  [x & {:keys [order-pattern] :as opts}]
  (.dumpToString (new-dumper opts) (cond-> x order-pattern (make-ordered order-pattern))))
