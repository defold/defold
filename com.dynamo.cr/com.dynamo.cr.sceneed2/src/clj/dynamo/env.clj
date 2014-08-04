(ns dynamo.env)

(def ^:dynamic *current-project* nil)

(defn current-project [] *current-project*)

(defmacro with-project [p & body]
  `(binding [*current-project* ~p]
     ~@body))