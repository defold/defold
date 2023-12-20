;; Copyright 2020-2023 The Defold Foundation
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
  (:import [java.lang.reflect Constructor Method Modifier]))

(set! *warn-on-reflection* true)

(defonce no-args-array (make-array Object 0))

(defonce no-classes-array (make-array Class 0))

(defn- get-declared-methods-raw [^Class class]
  (.getDeclaredMethods class))

(def get-declared-methods (memoize get-declared-methods-raw))

(defn- get-declared-constructor-raw [^Class class args-classes]
  {:pre [(counted? args-classes)]}
  (.getDeclaredConstructor class (if (zero? (count args-classes))
                                   no-classes-array
                                   (into-array Class args-classes))))

(def get-declared-constructor (memoize get-declared-constructor-raw))

(defn- get-declared-method-raw [^Class class ^String method-name args-classes]
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
