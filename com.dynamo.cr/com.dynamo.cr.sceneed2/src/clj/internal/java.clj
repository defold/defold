(ns internal.java)

(defn invoke-no-arg-class-method
  [class method]
  (-> class (.getDeclaredMethod method (into-array Class []))
      (.invoke nil (into-array Object []))))
