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

(ns mem
  (:require [dynamo.graph :as g]
            [util.coll :as coll :refer [pair]])
  (:import [clojure.lang Keyword Symbol]
           [internal.node NodeTypeRef]
           [java.util List Locale]
           [org.github.jamm FieldAndClassFilter FieldFilter Filters MemoryMeter MemoryMeterListener$Factory MemoryMeterStrategy]
           [org.github.jamm.listeners NoopMemoryMeterListener TreePrinter$Factory]
           [org.github.jamm.strategies MemoryMeterStrategies]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-strategy
  ^MemoryMeterStrategy [^List guesses]
  (-> (MemoryMeterStrategies/getInstance)
      (.getStrategy guesses)))

(defn- as-ignored-class-filter
  ^FieldAndClassFilter [ignored-classes]
  (when-some [^List ignored-classes (some-> ignored-classes not-empty vec)]
    (if-not (every? class? ignored-classes)
      (throw (IllegalArgumentException. "ignored-classes must be a list of Class values."))
      (reify FieldAndClassFilter
        (ignore [_this class]
          (not= -1 (.indexOf ignored-classes class)))

        (ignore [this class _field]
          (not= -1 (.indexOf ignored-classes class)))))))

(defn- compose-class-filters
  ^FieldAndClassFilter [field-filter ^FieldAndClassFilter class-filter]
  (cond
    (not (instance? FieldAndClassFilter class-filter))
    (throw (IllegalArgumentException. "class-filter must be a FieldAndClassFilter."))

    (instance? FieldAndClassFilter field-filter)
    (reify FieldAndClassFilter
      (ignore [_this class]
        (or (.ignore class-filter class)
            (.ignore ^FieldAndClassFilter field-filter class)))

      (ignore [_this class field]
        (or (.ignore class-filter class)
            (.ignore ^FieldAndClassFilter field-filter class field))))

    (instance? FieldFilter field-filter)
    (reify FieldAndClassFilter
      (ignore [_this class]
        (.ignore class-filter class))

      (ignore [_this class field]
        (or (.ignore class-filter class)
            (.ignore ^FieldFilter field-filter class field))))

    :else
    (throw (IllegalArgumentException. "field-filter must be a FieldFilter or a FieldAndClassFilter."))))

(defn- make-class-filter
  ^FieldAndClassFilter [ignore-known-singletons ignored-class-filter]
  (cond-> (Filters/getClassFilters ignore-known-singletons)
          ignored-class-filter (compose-class-filters ignored-class-filter)))

(defn- make-field-filter
  ^FieldFilter [ignore-known-singletons ignore-outer-class-reference ignore-non-strong-references ignored-class-filter]
  (cond-> (Filters/getFieldFilters ignore-known-singletons ignore-outer-class-reference ignore-non-strong-references)
          ignored-class-filter (compose-class-filters ignored-class-filter)))

(defn- make-listener-factory
  ^MemoryMeterListener$Factory [debug]
  (cond
    (false? debug)
    NoopMemoryMeterListener/FACTORY

    (true? debug)
    (TreePrinter$Factory. Integer/MAX_VALUE)

    (and (integer? debug) (pos? (int debug)))
    (TreePrinter$Factory. (int debug))

    :else
    (throw (IllegalArgumentException. "debug must be a boolean or a positive integer depth."))))

(defn- validate-option-keys! [options allowed-keys]
  (some->> allowed-keys
           (reduce #(dissoc %1 %2) options)
           (not-empty)
           (hash-map :unknown-options)
           (ex-info "Unknown entries in options map.")
           (throw)))

(def ^:private make-memory-meter-option-keys
  #{:debug
    :ignore-known-singletons
    :ignore-non-strong-references
    :ignore-outer-class-reference
    :ignored-classes})

(defn- make-memory-meter-impl
  ^MemoryMeter [{:keys [debug
                        ignore-known-singletons
                        ignore-non-strong-references
                        ignore-outer-class-reference
                        ignored-classes]
                 :or {debug false
                      ignore-known-singletons true
                      ignore-non-strong-references true
                      ignore-outer-class-reference false}}]
  (let [strategy (make-strategy MemoryMeter/BEST)
        ignored-class-filter (as-ignored-class-filter ignored-classes)
        class-filter (make-class-filter ignore-known-singletons ignored-class-filter)
        field-filter (make-field-filter ignore-known-singletons ignore-outer-class-reference ignore-non-strong-references ignored-class-filter)
        listener-factory (make-listener-factory debug)]
    (MemoryMeter. strategy class-filter field-filter listener-factory)))

(defn make-memory-meter
  "Create a new MemoryMeter instance.

  Supported options:
    :debug
    If true, print the object layout tree to stdout. Can also be set to a number
    to limit the nesting level being printed.

    :ignore-known-singletons
    If false, include known singletons such as Enum, Class, ClassLoader and
    AccessControlContext in the measurement.

    :ignore-non-strong-references
    If false, include references from weak references in the measurement.

    :ignore-outer-class-reference
    If true, ignores the outer class reference from non-static inner classes. In
    practice this is only useful if the object provided to the measure function
    is an instance of an inner class, and we wish to exclude outer class from
    the measurement.

    :ignored-classes
    Seq of Class values to exclude from the measurement. The class hierarchy is
    not considered, so you need to explicitly list the concrete classes to
    exclude."
  ^MemoryMeter [options]
  (validate-option-keys! options make-memory-meter-option-keys)
  (make-memory-meter-impl options))

(defn size-text
  "Return a human-readable string expressing the supplied byte-size using a
  suitable SI unit."
  ^String [^long byte-size]
  (if (< byte-size 1000)
    (str byte-size " B") ; Exact byte size.
    (let [units "EPTGMK"]
      (loop [size (/ byte-size 1000.0)
             unit-index (dec (count units))]
        (if (or (< size 1000.0)
                (zero? unit-index))
          (String/format Locale/ROOT "%.1f %cB" (to-array [size (.charAt units unit-index)]))
          (recur (/ size 1000.0)
                 (dec unit-index)))))))

(def ^:private measure-option-keys
  (into make-memory-meter-option-keys
        #{:meter
          :shallow
          :bytes}))

(defn measure
  "Measure the memory usage of the supplied object. By default, the size is
  returned as a human-readable string. If supplied, the map of options can be
  used to control the return value and other details of the measurement.

  Supported options:
    :meter
    Custom MemoryMeter. If not specified, use default.

    :shallow
    If true, count only the object header and its fields, don't follow object
    references.

    :bytes
    If true, return a number of bytes instead of a string.

    :debug
    If true, print the object layout tree to stdout. Can also be set to a number
    to limit the nesting level being printed.

    :ignore-known-singletons
    If false, include known singletons such as Enum, Class, ClassLoader and
    AccessControlContext in the measurement.

    :ignore-non-strong-references
    If false, include references from weak references in the measurement.

    :ignore-outer-class-reference
    If true, ignores the outer class reference from non-static inner classes. In
    practice this is only useful if the object provided to the measure function
    is an instance of an inner class, and we wish to exclude outer class from
    the measurement.

    :ignored-classes
    Seq of Class values to exclude from the measurement."
  ([object] (measure object nil))
  ([object {:keys [bytes meter shallow] :as options}]
   (validate-option-keys! options measure-option-keys)
   (let [^MemoryMeter memory-meter (or meter (make-memory-meter-impl options))
         byte-size (if shallow
                     (.measure memory-meter object)
                     (.measureDeep memory-meter object))]
     (cond-> byte-size (not bytes) size-text))))

(defn size-report
  "Given a sequence of objects and an object categorization function, returns a
  sorted list of total memory allocated for each category in descending order.
  The memory is reported as a human-readable string."
  ([category-fn objects]
   (size-report category-fn nil objects))
  ([category-fn ignored-classes objects]
   (let [memory-meter (make-memory-meter
                        {:ignored-classes (into [Keyword
                                                 Symbol]
                                                ignored-classes)})
         measure-opts {:bytes true
                       :meter memory-meter}]
     (->> objects
          (keep (juxt category-fn #(measure % measure-opts)))
          (coll/aggregate-into {} +)
          (map coll/flip)
          (sort-by first coll/descending-order)
          (mapv (fn [[byte-size category]]
                  (pair (size-text byte-size)
                        category)))))))

(defn node-size-report
  "Returns a sorted list of what node types are referencing the most memory in
  the system graph in the format [byte-size node-type-kw]. The list is sorted by
  total bytes allocated in descending order."
  []
  (->> (:graphs @g/*the-system*)
       (vals)
       (coll/mapcat #(vals (:nodes %)))
       (size-report #(:k (g/node-type %)) [NodeTypeRef])))
