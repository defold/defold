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

(ns editor.icons
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [service.log :as log]
            [util.thread-util :as thread-util])
  (:import [java.net URL]
           [javafx.scene.image Image ImageView]))

(defonce cached-icons-atom (atom {}))
(defonce workspace-atom (atom nil))

(defn initialize! [workspace]
  (assert (integer? workspace))
  (reset! workspace-atom workspace))

(defn- load-bundled-image
  ^Image [^URL bundled-resource-url ^Double size]
  (if (some? size)
    (Image. (str bundled-resource-url) size size true true)
    (Image. (str bundled-resource-url))))

(defn- load-workspace-image
  ^Image [resource ^Double size]
  (try
    (with-open [input-stream (io/input-stream resource)]
      (if (some? size)
        (Image. input-stream size size true true)
        (Image. input-stream)))
    (catch Exception exception
      (let [msg (str "Failed to load icon `" name "` from workspace.\n")]
        (log/error :msg msg :exception exception)
        nil))))

(defn- find-resource
  ([workspace proj-path]
   (g/with-auto-evaluation-context evaluation-context
     (find-resource workspace proj-path evaluation-context)))
  ([workspace proj-path evaluation-context]
   (get (g/node-value workspace :resource-map evaluation-context) proj-path)))

(defn- load-icon-image
  ^Image [^String name ^Double size]
  (if-some [bundled-resource-url (io/resource name)]
    (load-bundled-image bundled-resource-url size)
    (when-some [workspace @workspace-atom]
      (when-some [resource (find-resource workspace name)]
        (load-workspace-image resource size)))))

(defn get-image
  (^Image [name]
   (get-image name nil))
  (^Image [name ^Double size]
   (let [icon-key [name size]]
     (if-some [cached-entry (find @cached-icons-atom icon-key)]
       (val cached-entry)
       (first
         (thread-util/swap-rest!
           cached-icons-atom
           (fn [cached-icons]
             (let [icon-image (load-icon-image (str name) size)
                   updated-cached-icons (assoc cached-icons icon-key icon-image)]
               [updated-cached-icons icon-image]))))))))

(defn get-image-view
  (^ImageView [name]
   (ImageView. (get-image name)))
  (^ImageView [name ^Double size]
   (doto (ImageView. (get-image name))
     (.setFitWidth size)
     (.setFitHeight size))))
