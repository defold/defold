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

(ns editor.id
  (:refer-clojure :exclude [resolve])
  (:require [util.coll :as coll]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- ids->lookup [ids]
  (if (or (set? ids) (map? ids))
    ids
    (set ids)))

(defn- gen-impl [basename lookup]
  (loop [postfix 0]
    (let [name (if (= postfix 0) basename (str basename postfix))]
      (if (contains? lookup name)
        (recur (inc postfix))
        name))))

(defn gen
  "Generate unique id that does not intersect with taken ids

  Returns an id

  Args:
    basename     a base name for an id to generate, e.g. \"go\"
    taken-ids    collection of taken ids, either a map with id keys, a set, or
                 another type of id collection that will be coerced to set"
  [basename taken-ids]
  (gen-impl basename (ids->lookup taken-ids)))

(defn- trim-digits
  ^String [^String id]
  (loop [index (.length id)]
    (if (zero? index)
      ""
      (if (Character/isDigit (.charAt id (unchecked-dec index)))
        (recur (unchecked-dec index))
        (subs id 0 index)))))

(defn resolve
  "Resolve a wanted id to an id that does not intersect with taken ids

  Only performs a rename if there is a conflict. Returns an id

  Args:
    wanted-id    wanted id, a string with an optional number suffix that will be
                 stripped
    taken-ids    a collection of taken ids, either a map with id keys, a set, or
                 another type of id collection that will be coerced to set"
  [wanted-id taken-ids]
  (let [lookup (ids->lookup taken-ids)]
    (if (contains? lookup wanted-id)
      (gen (trim-digits wanted-id) lookup)
      wanted-id)))

(defn resolve-all
  "Resolve all wanted ids to ids that don't intersect with taken ids

  Only performs renames if there are conflicts. Returns a vector of ids, one for
  each input wanted ids

  Args:
    wanted-ids    a sequential collection of wanted ids, i.e., strings with
                  optional number suffixes that will be stripped
    taken-ids     a collection of taken ids, either a map with id keys, a set,
                  or another type of id collection that will be coerced to set"
  [wanted-ids taken-ids]
  (persistent!
    (key
      (reduce
        (fn [e wanted-id]
          (let [lookup (val e)
                id (if (contains? lookup wanted-id)
                     (gen-impl (trim-digits wanted-id) lookup)
                     wanted-id)]
            (coll/pair (conj! (key e) id)
                       (cond
                         (set? lookup) (conj lookup id)
                         (map? lookup) (assoc lookup id id)
                         :else (throw (AssertionError. (str "lookup is neither set nor map: " (class lookup))))))))
        (coll/pair
          (transient [])
          (ids->lookup taken-ids))
        wanted-ids))))
