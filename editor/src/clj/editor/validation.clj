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

(ns editor.validation
  (:require [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [util.coll :as coll]
            [util.path :as path]))

(set! *warn-on-reflection* true)

(defmacro blend-mode-tip [field pb-blend-type]
  `(g/fnk [~field]
     (when (= ~field :blend-mode-add-alpha)
       (let [options# (protobuf/enum-values ~pb-blend-type)
             options# (zipmap (map first options#) (map (comp :display-name second) options#))]
         (localization/message "error.blend-mode-add-alpha-replaced"
                               {"old" (options# :blend-mode-add-alpha)
                                "new" (options# :blend-mode-add)})))))

(defn format-ext-message [ext]
  (if (coll? ext)
    (localization/or-list (vec (sort (mapv (partial str \.) ext))))
    (str \. ext)))

(defn format-name
  ^String [name]
  (-> name
      camel/->Camel_Snake_Case_String
      (clojure.string/replace "_" " ")
      clojure.string/trim))

(defn prop-id-duplicate? [id-counts id]
  (when (> (id-counts id) 1)
    (localization/message "error.duplicate-instance-id" {"id" id})))

(defn prop-contains-prohibited-characters? [id name]
  (cond
    (coll/empty? id)
    nil

    (re-find #"[#:]" id)
    (localization/message "error.property-contains-url-prohibited-characters" {"property" name})

    (or (= (first id) \space) (= (last id) \space))
    (localization/message "error.property-cannot-start-or-end-with-space" {"property" name})))

(defn prop-negative? [v name]
  (when (< v 0)
    (localization/message "error.property-cannot-be-negative" {"property" name})))

(defn prop-zero-or-below? [v name]
  (when (<= v 0)
    (localization/message "error.property-must-be-greater-than-zero" {"property" name})))

(defn prop-nil? [v name]
  (when (nil? v)
    (localization/message "error.unspecified-property" {"property" name})))

(defn prop-empty? [v name]
  (when (empty? v)
    (localization/message "error.unspecified-property" {"property" name})))

(defn prop-resource-not-exists? [v name]
  (and v
       (not (resource/exists? v))
       (if-some [symlink-target-path (some-> (path/symlink-target v) path/absolute)]
         (localization/message "error.property-resource-is-a-broken-symlink"
                               {"property" name
                                "resource" (resource/resource->proj-path v)
                                "path" symlink-target-path})
         (localization/message "error.property-resource-not-found"
                               {"property" name
                                "resource" (resource/resource->proj-path v)}))))

(defn prop-resource-missing? [v name]
  (or (prop-nil? v name)
      (prop-resource-not-exists? v name)))

(defn prop-resource-ext? [v ext name]
  (or (prop-resource-missing? v name)
      (when-not (= (resource/type-ext v) ext)
        (localization/message "error.resource-assignment-not-of-type"
                              {"property" name
                               "resource" (resource/resource->proj-path v)
                               "type" (format-ext-message ext)}))))

(defn prop-resource-not-component? [v name]
  (let [resource-type (some-> v resource/resource-type)
        tags (:tags resource-type)]
    (when-not (or (contains? tags :component)
                  (contains? tags :embeddable))
      (localization/message "error.only-components-allowed"
                            {"property" name
                             "ext" (:ext resource-type)}))))

(defn prop-member-of? [v val-set message]
  (when (and val-set (not (val-set v)))
    message))

(defn prop-anim-missing? [animation anim-ids in]
  (when (and anim-ids (not-any? #(= animation %) anim-ids))
    (localization/message "error.animation-not-found" {"animation" animation "property" in})))

(defn prop-anim-missing-in? [animation anim-data in]
  (when-not (contains? anim-data animation)
    (localization/message "error.animation-not-found" {"animation" animation "property" in})))

(defn prop-outside-range? [[min max] v name]
  (when-not (<= min v max)
    (localization/message "error.property-must-be-between" {"property" name "min" min "max" max})))

(defn prop-minimum-check? [min v name]
  (when (< v min)
    (localization/message "error.property-must-be-at-least" {"property" name "min" min})))

(defn prop-maximum-check? [max v name]
  (when (> v max)
    (localization/message "error.property-must-be-at-most" {"property" name "max" max})))

(defn prop-collision-shape-conflict? [shapes collision-shape]
  (when (and collision-shape (not (empty? shapes)))
    (localization/message "error.collision-shape-conflict")))

(def prop-0-1? (partial prop-outside-range? [0.0 1.0]))

(def prop-1-1? (partial prop-outside-range? [-1.0 1.0]))

(defn prop-error
  ([severity _node-id prop-kw f prop-value & args]
   (when-let [msg (apply f prop-value args)]
     (g/->error _node-id prop-kw severity prop-value msg {}))))

(defn keyword->name [kw]
  (-> kw
      name
      format-name))
