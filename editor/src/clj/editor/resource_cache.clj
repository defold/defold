;; Copyright 2021 The Defold Foundation
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

(ns editor.resource-cache
  (:require [clojure.core.cache :as cache]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io])
  (:import [clojure.lang MapEntry]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(defn- pair [a b]
  (MapEntry/create a b))

;; -----------------------------------------------------------------------------
;; Helper functions for implementing resource-based caches.
;; -----------------------------------------------------------------------------

(defn get-cache-value-or-exception
  [make-cache-value-or-exception-fn cache-atom project path-or-resource evaluation-context]
  (let [resource (project/get-resource project path-or-resource evaluation-context)]
    (when-some [resource-node-id (project/get-resource-node project resource evaluation-context)]
      (let [proj-path (resource/proj-path resource)
            cache-key (pair resource-node-id proj-path)]
        (-> cache-atom
            (swap! (fn cache-update-fn [cache]
                     (if (cache/has? cache cache-key)
                       (cache/hit cache cache-key)
                       (cache/miss cache cache-key (make-cache-value-or-exception-fn project resource evaluation-context)))))
            (cache/lookup cache-key))))))

(defn get-cache-value-or-exception-or-nil
  [cache-atom project path-or-resource evaluation-context]
  (let [resource (project/get-resource project path-or-resource evaluation-context)]
    (when-some [resource-node-id (project/get-resource-node project resource evaluation-context)]
      (let [proj-path (resource/proj-path resource)
            cache-key (pair resource-node-id proj-path)]
        (cache/lookup @cache-atom cache-key)))))

(defn get-cache-value-or-throw
  [get-cache-value-or-exception-fn project path-or-resource evaluation-context]
  (let [content-or-exception (get-cache-value-or-exception-fn project path-or-resource evaluation-context)]
    (if (instance? Exception content-or-exception)
      (throw content-or-exception)
      content-or-exception)))

(defn get-cache-value-or-error
  [get-cache-value-or-throw-fn path-or-resource referencing-node-id referencing-label evaluation-context]
  (let [basis (:basis evaluation-context)
        project (project/get-project basis referencing-node-id)
        resource (project/get-resource project path-or-resource evaluation-context)]
    (resource-io/with-error-translation resource referencing-node-id referencing-label
      (get-cache-value-or-throw-fn project resource evaluation-context))))

;; -----------------------------------------------------------------------------
;; Resource content cache
;; -----------------------------------------------------------------------------

(defonce ^:private resource-content-cache-entry-limit 128)
(defonce ^:private resource-content-cache-atom (atom (cache/lru-cache-factory {} :threshold resource-content-cache-entry-limit)))

(defn- read-bytes
  ^bytes [resource]
  (with-open [input-stream (io/input-stream resource)]
    (IOUtils/toByteArray input-stream)))

(defn- read-bytes-or-exception
  ^bytes [_project resource _evaluation-context]
  (try
    (read-bytes resource)
    (catch Exception exception
      exception)))

(defn get-resource-content-or-exception
  ^bytes [project path-or-resource evaluation-context]
  (get-cache-value-or-exception read-bytes-or-exception resource-content-cache-atom project path-or-resource evaluation-context))

(defn get-resource-content-or-throw
  ^bytes [project path-or-resource evaluation-context]
  (get-cache-value-or-throw get-resource-content-or-exception project path-or-resource evaluation-context))

(defn get-resource-content-or-error
  ^bytes [path-or-resource referencing-node-id referencing-label evaluation-context]
  (get-cache-value-or-error get-resource-content-or-throw path-or-resource referencing-node-id referencing-label evaluation-context))

;; -----------------------------------------------------------------------------
;; Path-inclusive SHA1 hex cache
;; -----------------------------------------------------------------------------

(defonce ^:private path-inclusive-sha1-hex-cache-entry-limit 1024)
(defonce ^:private path-inclusive-sha1-hex-cache-atom (atom (cache/lru-cache-factory {} :threshold path-inclusive-sha1-hex-cache-entry-limit)))

(defn- read-path-inclusive-sha1-hex-or-exception
  ^String [project resource evaluation-context]
  (let [content-or-exception (get-resource-content-or-exception project resource evaluation-context)]
    (if (instance? Exception content-or-exception)
      content-or-exception
      (let [proj-path-fn (fn proj-path-fn [_content] (resource/proj-path resource))]
        (resource/resource->path-inclusive-sha1-hex content-or-exception proj-path-fn)))))

(defn get-path-inclusive-sha1-hex-or-exception
  ^String [project path-or-resource evaluation-context]
  (get-cache-value-or-exception read-path-inclusive-sha1-hex-or-exception path-inclusive-sha1-hex-cache-atom project path-or-resource evaluation-context))

(defn get-path-inclusive-sha1-hex-or-throw
  (^String [project path-or-resource]
   (g/with-auto-evaluation-context evaluation-context
     (get-path-inclusive-sha1-hex-or-throw project path-or-resource evaluation-context)))
  (^String [project path-or-resource evaluation-context]
   (get-cache-value-or-throw get-path-inclusive-sha1-hex-or-exception project path-or-resource evaluation-context)))

(defn get-path-inclusive-sha1-hex-or-error
  (^String [path-or-resource referencing-node-id referencing-label]
   (g/with-auto-evaluation-context evaluation-context
     (get-path-inclusive-sha1-hex-or-error path-or-resource referencing-node-id referencing-label evaluation-context)))
  (^String [path-or-resource referencing-node-id referencing-label evaluation-context]
   (get-cache-value-or-error get-path-inclusive-sha1-hex-or-throw path-or-resource referencing-node-id referencing-label evaluation-context)))
