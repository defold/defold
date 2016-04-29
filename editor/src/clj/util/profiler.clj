(ns util.profiler
  (:import [com.defold.util Profiler]))

(set! *warn-on-reflection* true)

(defmacro profile
  [name user & body]
  `(let [s# (Profiler/begin ~name ~user)]
     (try
       ~@body
       (finally
         (Profiler/end s#)))))

(defn begin-frame []
  (Profiler/beginFrame))

(defn dump-json []
  (Profiler/dumpJson))
