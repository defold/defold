;; Copyright 2020 The Defold Foundation
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
  (:import [java.lang.reflect Method Modifier]))

(set! *warn-on-reflection* true)

(defn- get-declared-method-raw [^Class class method args-classes]
  (.getDeclaredMethod class method (into-array Class args-classes)))

(def get-declared-method (memoize get-declared-method-raw))

(def no-args-array (to-array []))

(defn invoke-no-arg-class-method
  [^Class class method]
  (-> class
    ^Method (get-declared-method method [])
    (.invoke nil no-args-array)))

(defn invoke-class-method
  [^Class class ^String method-name args]
  (-> class
    ^Method (get-declared-method class method-name (mapv class args))
    (.invoke nil (to-array args))))
