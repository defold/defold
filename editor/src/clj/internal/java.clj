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
