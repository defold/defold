(ns editor.profiler
  (:import [com.defold.editor Profiler]))

(defmacro profile
  [name user expr]
  `(let [s# (Profiler/begin ~name ~user)]
     (try
       ~expr
       (finally
         (Profiler/end s#)))))

(defn begin-frame []
  (Profiler/beginFrame))
