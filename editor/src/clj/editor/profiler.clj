(ns editor.profiler
  (:import [com.defold.editor Profiler]))

(defmacro profile
  [name user expr]
  `(let [s# (Profiler/begin ~name ~user)
         ret# ~expr]
     (Profiler/add s#)
     ret#))

(profile "foo2" 1 (Thread/sleep 1000))

(Profiler/dump "/tmp/foo")
