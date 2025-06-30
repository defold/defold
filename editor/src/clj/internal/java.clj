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

(ns internal.java
  (:import [clojure.lang DynamicClassLoader Util]
           [java.lang.reflect Constructor Field Method Modifier]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce no-args-array (make-array Object 0))

(defonce no-classes-array (make-array Class 0))

(defonce byte-array-class (.getClass (byte-array 0)))

(defn field-static? [^Field field]
  (Modifier/isStatic (.getModifiers field)))

(defn instance-fields-raw [^Class class]
  (filterv #(not (field-static? %))
           (.getFields class)))

(def instance-fields (memoize instance-fields-raw))

(defn- get-declared-methods-raw [^Class class]
  (.getDeclaredMethods class))

(def get-declared-methods (memoize get-declared-methods-raw))

(defn- get-declared-constructor-raw
  ^Constructor [^Class class args-classes]
  {:pre [(counted? args-classes)]}
  (.getDeclaredConstructor class (if (zero? (count args-classes))
                                   no-classes-array
                                   (into-array Class args-classes))))

(def get-declared-constructor (memoize get-declared-constructor-raw))

(defn- get-declared-method-raw
  ^Method [^Class class ^String method-name args-classes]
  {:pre [(counted? args-classes)]}
  (.getDeclaredMethod class method-name (if (zero? (count args-classes))
                                          no-classes-array
                                          (into-array Class args-classes))))

(def get-declared-method (memoize get-declared-method-raw))

(defn invoke-no-arg-constructor
  [^Class class]
  (-> class
      ^Constructor (get-declared-constructor [])
      (.newInstance no-args-array)))

(defn invoke-no-arg-class-method
  [^Class class ^String method-name]
  (-> class
      ^Method (get-declared-method method-name [])
      (.invoke nil no-args-array)))

(defn invoke-class-method
  [^Class class ^String method-name args]
  (-> class
      ^Method (get-declared-method method-name (mapv clojure.core/class args))
      (.invoke nil (to-array args))))

(defn public-implementation? [^Class subclass ^Class superclass]
  (let [modifiers (.getModifiers subclass)]
    (and (Modifier/isPublic modifiers)
         (not (Modifier/isAbstract modifiers))
         (not= superclass subclass)
         (isa? subclass superclass))))

;; Class loader used when loading editor extensions from libraries.
;; It's important to use the same class loader, so the type signatures match.

(def ^DynamicClassLoader class-loader
  (DynamicClassLoader. (.getContextClassLoader (Thread/currentThread))))

(defn load-class! [class-name]
  (Class/forName class-name true class-loader))

(defn- combine-comparisons-syntax [exp & rest-exps]
  (if (empty? rest-exps)
    exp
    (let [comparison-sym (gensym "comparison")]
      `(let [~comparison-sym ~exp]
         (if (zero? ~comparison-sym)
           ~(apply combine-comparisons-syntax rest-exps)
           ~comparison-sym)))))

(defmacro combine-comparisons [& exps]
  (apply combine-comparisons-syntax exps))

(defmacro combine-hashes [exp & rest-exps]
  (list* `-> exp
         (map #(list `Util/hashCombine %)
              rest-exps)))
