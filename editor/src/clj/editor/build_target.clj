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

(ns editor.build-target
  (:require [editor.resource :as resource]
            [editor.system :as system]
            [editor.workspace :as workspace]
            [util.coll :refer [pair]]
            [util.digestable :as digestable]))

(set! *warn-on-reflection* true)

(defn content-hash-components [build-target]
  (let [build-resource (:resource build-target)
        source-resource (:resource build-resource)]
    ;; The source resource is included since external resources may rely on it
    ;; (for example, image reads). However, we don't want to include memory
    ;; resources since they typically represent embedded resources that can be
    ;; edited, and their :data may be out of sync with the edited state that
    ;; will be passed as :user-data anyway.
    {:engine-sha1 (system/defold-engine-sha1)
     :source-resource (when-not (resource/memory-resource? source-resource)
                        source-resource)
     :build-fn (:build-fn build-target)
     :user-data (:user-data build-target)}))

(defn content-hash
  ^String [build-target opts]
  (let [content-hash-components (content-hash-components build-target)]
    (try
      (digestable/sha1-hash content-hash-components opts)
      (catch Throwable error
        (throw (ex-info (str "Failed to digest content for resource: " (resource/proj-path (:resource build-target)))
                        {:build-target build-target
                         :content-hash-components content-hash-components}
                        error))))))

(def content-hash? digestable/sha1-hash?)

(defn with-content-hash
  ([build-target]
   (with-content-hash build-target nil))
  ([build-target opts]
   (let [content-hash (content-hash build-target opts)]
     (cond-> (assoc build-target :content-hash content-hash)
             (resource/memory-resource? (:resource (:resource build-target)))
             (assoc-in [:resource :resource :data] content-hash)))))

(defn make-proj-path->build-target [build-targets]
  ;; Create a map that can be used to locate the build target that was produced
  ;; from a specific resource in the project. Embedded resources (i.e. MemoryResources)
  ;; are excluded from the map, since these have no source path in the project,
  ;; and it should not be possible for another resource to reference them.
  (into {}
        (keep (fn [build-target]
                (when-some [source-resource (-> build-target :resource :resource)]
                  (pair (resource/proj-path source-resource)
                        build-target))))
        (flatten build-targets)))
