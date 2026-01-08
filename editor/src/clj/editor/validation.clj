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
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.util :as util]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(defmacro blend-mode-tip [field pb-blend-type]
  `(g/fnk [~field]
     (when (= ~field :blend-mode-add-alpha)
       (let [options# (protobuf/enum-values ~pb-blend-type)
             options# (zipmap (map first options#) (map (comp :display-name second) options#))]
         (format "\"%s\" has been replaced by \"%s\"",
                 (options# :blend-mode-add-alpha) (options# :blend-mode-add))))))

(defn format-ext
  ^String [ext]
  (if (coll? ext)
    (util/join-words ", " " or " (into (sorted-set) (map (partial str \.)) ext))
    (str \. ext)))

(defn format-name
  ^String [name]
  (-> name
      camel/->Camel_Snake_Case_String
      (clojure.string/replace "_" " ")
      clojure.string/trim))

(defn prop-id-duplicate? [id-counts id]
  (when (> (id-counts id) 1)
    (format "'%s' is in use by another instance" id)))

(defn prop-contains-prohibited-characters? [id name]
  (cond
    (coll/empty? id)
    nil

    (re-find #"[#:]" id)
    (format "%s should not contain special URL symbols such as '#' or ':'" name)

    (or (= (first id) \space) (= (last id) \space))
    (format "%s should not start or end with a space symbol" name)))

(defn prop-negative? [v name]
  (when (< v 0)
    (format "'%s' cannot be negative" name)))

(defn prop-zero-or-below? [v name]
  (when (<= v 0)
    (format "'%s' must be greater than zero" name)))

(defn prop-nil? [v name]
  (when (nil? v)
    (format "'%s' must be specified" name)))

(defn prop-empty? [v name]
  (when (empty? v)
    (format "'%s' must be specified" name)))

(defn prop-resource-not-exists? [v name]
  (and v (not (resource/exists? v)) (format "%s '%s' could not be found" name (resource/resource->proj-path v))))

(defn prop-resource-missing? [v name]
  (or (prop-nil? v name)
      (prop-resource-not-exists? v name)))

(defn prop-resource-ext? [v ext name]
  (or (prop-resource-missing? v name)
      (when-not (= (resource/type-ext v) ext)
        (format "%s '%s' is not of type %s" name (resource/resource->proj-path v) (format-ext ext)))))

(defn prop-resource-not-component? [v name]
  (let [resource-type (some-> v resource/resource-type)
        tags (:tags resource-type)]
    (when-not (or (contains? tags :component)
                  (contains? tags :embeddable))
      (format "Only components allowed for '%s'. '%s' is not a component." name (:ext resource-type)))))

(defn prop-member-of? [v val-set message]
  (when (and val-set (not (val-set v)))
    message))

(defn prop-anim-missing? [animation anim-ids]
  (when (and anim-ids (not-any? #(= animation %) anim-ids))
    (format "'%s' could not be found in the specified image" animation)))

(defn prop-anim-missing-in? [animation anim-data in]
  (when-not (contains? anim-data animation)
    (format "'%s' could not be found in '%s'" animation in)))

(defn prop-outside-range? [[min max] v name]
  (let [tmpl (if (integer? min)
               "'%s' must be between %d and %d"
               "'%s' must be between %f and %f")]
    (when (not (<= min v max))
      (util/format* tmpl name min max))))

(defn prop-minimum-check? [min v name]
  (when (< v min)
    (let [tmpl (if (integer? min)
                 "'%s' must be at least %d"
                 "'%s' must be at least %f")]
      (util/format* tmpl name min))))

(defn prop-maximum-check? [max v name]
  (when (> v max)
    (let [tmpl (if (integer? max)
                 "'%s' must be at most %d"
                 "'%s' must be at most %f")]
      (util/format* tmpl name max))))

(defn prop-collision-shape-conflict? [shapes collision-shape]
  (when (and collision-shape (not (empty? shapes)))
    "Cannot combine embedded shapes with a referenced 'Collision Shape'. Please remove either."))

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

(defmacro prop-error-fnk
  [severity f property]
  (let [name-kw# (keyword property)
        name# (keyword->name name-kw#)]
    `(g/fnk [~'_node-id ~property]
            (prop-error ~severity ~'_node-id ~name-kw# ~f ~property ~name#))))
