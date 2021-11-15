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

(ns editor.image-cache
  (:require [clojure.core.cache :as cache]
            [dynamo.graph :as g]
            [editor.image-util :as image-util]
            [editor.resource-cache :as resource-cache])
  (:import [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Image cache
;; -----------------------------------------------------------------------------

(defonce ^:private image-cache-entry-limit 128)
(defonce ^:private image-cache-atom (atom (cache/lru-cache-factory {} :threshold image-cache-entry-limit)))

(defn- read-image
  ^BufferedImage [project resource evaluation-context]
  (image-util/read-image
    (resource-cache/get-resource-content-or-throw project resource evaluation-context)))

(defn- read-image-or-exception
  [project resource evaluation-context]
  (try
    (read-image project resource evaluation-context)
    (catch Exception exception
      exception)))

(defn get-image-or-exception
  ^BufferedImage [project path-or-resource evaluation-context]
  (resource-cache/get-cache-value-or-exception read-image-or-exception image-cache-atom project path-or-resource evaluation-context))

(defn get-image-or-throw
  ^BufferedImage [project path-or-resource evaluation-context]
  (resource-cache/get-cache-value-or-throw get-image-or-exception project path-or-resource evaluation-context))

(defn get-image-or-error
  ([path-or-resource referencing-node-id referencing-label]
   (g/with-auto-evaluation-context evaluation-context
     (get-image-or-error path-or-resource referencing-node-id referencing-label evaluation-context)))
  ([path-or-resource referencing-node-id referencing-label evaluation-context]
   (resource-cache/get-cache-value-or-error get-image-or-throw path-or-resource referencing-node-id referencing-label evaluation-context)))

;; -----------------------------------------------------------------------------
;; Image size cache
;; -----------------------------------------------------------------------------

(defonce ^:private image-size-cache-entry-limit 8192)
(defonce ^:private image-size-cache-atom (atom (cache/lru-cache-factory {} :threshold image-size-cache-entry-limit)))

(defn- read-image-size
  ^BufferedImage [project resource evaluation-context]
  (let [resource-content-or-exception (resource-cache/get-resource-content-or-exception project resource evaluation-context)]
    (if (instance? Exception resource-content-or-exception)
      resource-content-or-exception
      (image-util/read-size resource-content-or-exception))))

(defn- read-image-size-or-exception
  [project resource evaluation-context]
  (try
    (read-image-size project resource evaluation-context)
    (catch Exception exception
      exception)))

(defn get-image-size-or-exception
  ^BufferedImage [project path-or-resource evaluation-context]
  (resource-cache/get-cache-value-or-exception read-image-size-or-exception image-size-cache-atom project path-or-resource evaluation-context))

(defn get-image-size-or-throw
  ^BufferedImage [project path-or-resource evaluation-context]
  (resource-cache/get-cache-value-or-throw get-image-size-or-exception project path-or-resource evaluation-context))

(defn get-image-size-or-error
  (^BufferedImage [path-or-resource referencing-node-id referencing-label]
   (g/with-auto-evaluation-context evaluation-context
     (get-image-size-or-error path-or-resource referencing-node-id referencing-label evaluation-context)))
  (^BufferedImage [path-or-resource referencing-node-id referencing-label evaluation-context]
   (resource-cache/get-cache-value-or-error get-image-size-or-throw path-or-resource referencing-node-id referencing-label evaluation-context)))
