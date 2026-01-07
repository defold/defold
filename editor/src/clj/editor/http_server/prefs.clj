;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.http-server.prefs
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.prefs :as prefs]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.http-server :as http-server])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(defn- get-prefs-path [request]
  (let [^String s (-> request :path-params :path)
        n (.length s)
        add-non-empty! (fn add-non-empty! [acc ^String s]
                         (cond-> acc (not (.isEmpty s)) (conj! (keyword s))))]
    (loop [acc (transient [])
           from 0
           to 0]
      (if (= n to)
        (persistent! (add-non-empty! acc (.substring s from to)))
        (let [ch (.charAt s to)]
          (cond
            (= ch \/)
            (recur (add-non-empty! acc (.substring s from to)) (inc to) (inc to))

            (prefs-docs/allowed-keyword-character? ch)
            (recur acc from (inc to))

            :else
            (throw (http-server/error (http-server/response 400 (format "Invalid prefs path character: '%s'" ch))))))))))

(defn- handle-get-prefs [prefs request]
  (try
    (http-server/json-response
      (prefs/get prefs (get-prefs-path request)))
    (catch ExceptionInfo e
      (if (::prefs/error (ex-data e))
        (http-server/response 400 (.getMessage e))
        (throw e)))))

(defonce/type JsonParseError [json message]
  Object
  (toString [_]
    (str (json/write-str json) " " message)))

(defn- json-parse-error? [x]
  (instance? JsonParseError x))

(defn- parse-json
  "Parse JSON value with string keys to a valid prefs value or a JsonParseError"
  [json schema]
  (case (:type schema)
    :boolean (cond-> json (not (boolean? json)) (->JsonParseError "is not a boolean"))
    (:string :password :locale) (cond-> json (not (string? json)) (->JsonParseError "is not a string"))
    :keyword (if (string? json) (keyword json) (->JsonParseError json "is not a string"))
    :integer (cond-> json (not (int? json)) (->JsonParseError "is not an integer"))
    :number (if (number? json) (double json) (->JsonParseError json "is not a number"))
    :one-of (let [{:keys [schemas]} schema
                  n (count schemas)]
              (loop [i 0]
                (if (= i n)
                  (->JsonParseError json "is invalid")
                  (let [result (parse-json json (schemas i))]
                    (if (json-parse-error? result)
                      (recur (inc i))
                      result)))))
    :array (if (vector? json)
             (let [{:keys [item]} schema]
               (coll/transfer json []
                 (map #(parse-json % item))
                 (halt-when json-parse-error?)))
             (->JsonParseError json "is not an array"))
    :set (if (vector? json)
           (let [{:keys [item]} schema]
             (coll/transfer json #{}
               (map #(parse-json % item))
               (halt-when json-parse-error?)))
           ;; sets should be represented as arrays in JSON
           (->JsonParseError json "is not an array"))
    :object (if (map? json)
              (let [{:keys [properties]} schema]
                (coll/transfer json {}
                  (map (fn [[k v]]
                         (let [kw (keyword k)]
                           (if-let [property-schema (kw properties)]
                             (let [result (parse-json v property-schema)]
                               (if (json-parse-error? result)
                                 result
                                 (coll/pair kw result)))
                             (->JsonParseError k "is not a valid property key")))))
                  (halt-when json-parse-error?)))
              (->JsonParseError json "is not an object"))
    :object-of (if (map? json)
                 (let [{:keys [key val]} schema]
                   (coll/transfer json {}
                     (map (fn [[k v]]
                            (let [k-result (parse-json k key)]
                              (if (json-parse-error? k-result)
                                k-result
                                (let [v-result (parse-json v val)]
                                  (if (json-parse-error? v-result)
                                    v-result
                                    (coll/pair k-result v-result)))))))
                     (halt-when json-parse-error?)))
                 (->JsonParseError json "is not an object"))
    :enum (let [{:keys [values]} schema
                n (count values)]
            (loop [i 0]
              (if (= i n)
                (->JsonParseError json "is invalid")
                (let [v (values i)]
                  (if (= json (json/read-str (json/write-str v)))
                    v
                    (recur (inc i)))))))
    :tuple (if (vector? json)
             (let [{:keys [items]} schema
                   n (count items)]
               (if (= n (count json))
                 (coll/transfer (range n) []
                   (map #(parse-json (json %) (items %)))
                   (halt-when json-parse-error?))
                 (->JsonParseError json (format "should have %s elements" n))))
             (->JsonParseError json "is not an array"))))

(defn- handle-post-prefs [prefs request]
  (try
    (let [path (get-prefs-path request)
          json (with-open [r (io/reader (:body request))]
                 (try
                   (json/read r)
                   (catch Exception e (throw (http-server/error (http-server/response 400 (.getMessage e)))))))
          value (parse-json json (prefs/schema prefs path))]
      (if (json-parse-error? value)
        (http-server/response 400 (str value))
        (do
          (prefs/set! prefs path value)
          (http-server/response 200))))
    (catch ExceptionInfo e
      (if (::prefs/error (ex-data e))
        (http-server/response 400 (.getMessage e))
        (throw e)))))

(defn routes [prefs]
  {"/prefs/{*path}" {"GET" (partial #'handle-get-prefs prefs)
                     "POST" (partial #'handle-post-prefs prefs)}})
