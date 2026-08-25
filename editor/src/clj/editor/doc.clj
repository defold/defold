;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.doc
  (:require [clojure.string :as string]
            [editor.fs :as fs]
            [editor.protobuf :as protobuf]
            [internal.java :as java]
            [service.log :as log]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.text-util :as text-util])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]
           [clojure.lang IReduceInit]
           [java.io IOException]
           [java.nio.charset MalformedInputException]
           [java.util.regex Pattern]))

(def ^:private ref-index
  (delay
    (->> "doc"
         (fs/class-path-walker java/class-loader)
         (e/mapcat
           (fn [path]
             (when-let [{:keys [info elements]}
                        (try
                          (protobuf/read-map-with-defaults ScriptDoc$Document path)
                          (catch MalformedInputException _
                            (log/warn :message "Ignoring legacy-format documentation file." :path (str path))
                            nil)
                          (catch IOException e
                            (log/error :message "Failed to read documentation file." :path (str path) :exception e)
                            nil))]
               (e/map (fn [element]
                        (-> element
                            (assoc :environment (if (= "editor" (:namespace info)) :editor :runtime))
                            (cond-> (string/blank? (:language element)) (assoc :language (:language info)))))
                      elements))))
         vec
         (sort-by :name)
         vec)))

(defn- all-matches [^Pattern re s]
  (reify IReduceInit
    (reduce [_ rf init]
      (let [matcher (re-matcher re s)]
        (loop [result init]
          (if (.find matcher)
            (let [result (rf result (.group matcher))]
              (if (reduced? result)
                (unreduced result)
                (recur result)))
            result))))))

(defn- combine-preds [combine-fn preds]
  (case (count preds)
    0 fn/constantly-true
    1 (first preds)
    (apply combine-fn preds)))

(defn- query-string->element-predicate [query]
  (let [{:keys [environment language q]}
        (into {}
              (map
                (fn [s]
                  (let [[k v] (string/split s #"=" 2)]
                    (coll/pair (keyword k) (or v "")))))
              (string/split query #"&"))

        environment-pred
        (when environment
          (let [value-preds
                (mapv (fn [environment]
                        (let [environment (keyword environment)]
                          (fn environment-value-pred [element]
                            (= environment (:environment element)))))
                      (all-matches #"[^,\s]+" environment))]
            (combine-preds some-fn value-preds)))

        language-pred
        (when language
          (let [value-preds
                (mapv (fn [language]
                        (fn language-value-pred [element]
                          (= language (:language element))))
                      (all-matches #"[^,\s]+" language))]
            (combine-preds some-fn value-preds)))

        q-pred
        (when q
          (let [alternative-preds
                (into []
                      (keep
                        (fn [alternative]
                          (let [term-preds
                                (mapv (fn [term]
                                        (fn q-term-pred [element]
                                          (coll/any?
                                            #(text-util/includes-ignore-case? % term)
                                            [(:name element)
                                             (:brief element)
                                             (:description element)])))
                                      (all-matches #"\S+" alternative))]
                            (when-not (zero? (count term-preds))
                              (combine-preds every-pred term-preds)))))
                      (string/split q #"\|"))]
            (combine-preds some-fn alternative-preds)))]
    (combine-preds
      every-pred
      (cond-> []
        environment-pred (conj environment-pred)
        language-pred (conj language-pred)
        q-pred (conj q-pred)))))

(defn- get-ref
  {:openapi
   {:summary "Runtime and editor API reference"
    :parameters [{:name "environment"
                  :in "query"
                  :description "`editor` or `runtime`, comma-separated."
                  :schema {:type "string"}}
                 {:name "language"
                  :in "query"
                  :description "`Lua`, `C`, or `C++`, comma-separated."
                  :schema {:type "string"}}
                 {:name "q"
                  :in "query"
                  :description "Case-insensitive search; `|` is OR, whitespace is AND."
                  :schema {:type "string"}}]
    :responses {"200" {:description "Array of script doc elements."
                       :content {"application/json" {}}}}}}
  [request]
  (http-server/json-response
    (filterv (query-string->element-predicate (:query request "")) @ref-index)))

(defn routes []
  {"/ref" {"GET" #'get-ref}})
