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
            [util.http-server :as http-server])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(defn- get-prefs-path [request]
  (let [^String s (-> request :path-params :path)
        n (.length s)
        add-non-empty! (fn add-non-empty! [acc ^String s]
                         (cond-> acc (not (.isEmpty s)) (conj! (keyword s))))
        ret (loop [acc (transient [])
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
                    (throw (http-server/error (http-server/response 400 (format "Invalid prefs path character: '%s'" ch))))))))]
    (if (coll/empty? ret)
      (throw (http-server/error (http-server/response 400 "No prefs path specified")))
      ret)))

(defn- handle-get-prefs [prefs request]
  (try
    (http-server/json-response
      (prefs/get prefs (get-prefs-path request)))
    (catch ExceptionInfo e
      (if (::prefs/error (ex-data e))
        (http-server/response 400 (.getMessage e))
        (throw e)))))

(defn- json->value
  "Coerce JSON value with string keys to prefs value"
  [json schema]
  (case (:type schema)
    (:any :string :password :locale :boolean :integer) json
    :keyword (cond-> json (string? json) keyword)
    :number (cond-> json (number? json) double)
    :array (if (vector? json)
             (let [item-schema (:item schema)]
               (mapv #(json->value % item-schema) json))
             json)
    :set (if (vector? json)
           (let [item-schema (:item schema)]
             (into #{} (map #(json->value % item-schema)) json))
           json)
    :object (if (map? json)
              (let [property-schemas (:properties schema)]
                (persistent!
                  (reduce-kv
                    (fn [acc k v]
                      (let [kw (keyword k)
                            property-schema (kw property-schemas)]
                        (assoc! acc kw (cond-> v property-schema (json->value property-schema)))))
                    (transient {})
                    json)))
              json)
    :object-of (if (map? json)
                 (let [key-schema (:key schema)
                       val-schema (:val schema)]
                   (persistent!
                     (reduce-kv
                       (fn [acc k v]
                         (assoc! acc (json->value k key-schema) (json->value v val-schema)))
                       (transient {})
                       json)))
                 json)
    :enum (let [->value (coll/pair-map-by (comp json/read-str json/write-str) identity (:values schema))]
            (->value json json))
    :tuple (if (vector? json)
             (let [item-schemas (:items schema)]
               (if (= (count json) (count item-schemas))
                 (mapv json->value json (:items schema))
                 json))
             json)))

(defn- handle-post-prefs [prefs request]
  (try
    (let [path (get-prefs-path request)
          value (with-open [r (io/reader (:body request))]
                  (try
                    (json/read r)
                    (catch Exception e (throw (http-server/error (http-server/response 400 (.getMessage e)))))))]
      (prefs/set! prefs path (json->value value (prefs/schema prefs path)))
      (http-server/response 200))
    (catch ExceptionInfo e
      (if (::prefs/error (ex-data e))
        (http-server/response 400 (.getMessage e))
        (throw e)))))

(defn routes [prefs]
  {"/prefs/{*path}" {"GET" (partial #'handle-get-prefs prefs)
                     "POST" (partial #'handle-post-prefs prefs)}})
