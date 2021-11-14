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
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [dynamo.graph :as g])
  (:import [clojure.lang MapEntry]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(defonce ^:private max-cached-resource-contents 128)
(defonce ^:private cache-atom (atom (cache/lru-cache-factory {} :threshold max-cached-resource-contents)))

(defn- pair [a b]
  (MapEntry/create a b))

(defn- read-bytes
  ^bytes [resource]
  (with-open [input-stream (io/input-stream resource)]
    (IOUtils/toByteArray input-stream)))

(defn get-resource-content
  ^bytes [project path-or-resource evaluation-context]
  (let [resource (project/get-resource project path-or-resource evaluation-context)]
    (when-some [resource-node-id (project/get-resource-node project resource evaluation-context)]
      (let [proj-path (resource/proj-path resource)
            cache-key (pair resource-node-id proj-path)]
        (-> cache-atom
            (swap! (fn [cache]
                     (if (cache/has? cache cache-key)
                       (cache/hit cache cache-key)
                       (cache/miss cache cache-key (read-bytes resource)))))
            (cache/lookup cache-key))))))

(defn read-resource-content [path-or-resource referencing-node-id referencing-label evaluation-context read-content-fn]
  (let [basis (:basis evaluation-context)
        project (project/get-project basis referencing-node-id)
        resource (project/get-resource project path-or-resource evaluation-context)]
    (resource-io/with-error-translation resource referencing-node-id referencing-label
      (let [content (get-resource-content project resource evaluation-context)]
        (read-content-fn content)))))

(defn path-inclusive-sha1-hex
  (^String [path-or-resource referencing-node-id]
   (g/with-auto-evaluation-context evaluation-context
     (path-inclusive-sha1-hex path-or-resource referencing-node-id evaluation-context)))
  (^String [path-or-resource referencing-node-id evaluation-context]
   (let [basis (:basis evaluation-context)
         project (project/get-project basis referencing-node-id)
         resource (project/get-resource project path-or-resource evaluation-context)
         content (get-resource-content project resource evaluation-context)
         proj-path-fn (fn proj-path-fn [_content] (resource/proj-path resource))]
     (resource/resource->path-inclusive-sha1-hex content proj-path-fn))))
