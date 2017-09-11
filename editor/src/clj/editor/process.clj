(ns editor.process
  (:require [clojure.java.io :as io])
  (:import [java.lang Process ProcessBuilder]))

(set! *warn-on-reflection* true)

(defn start! ^Process [^String command args opts]
  (let [pb (ProcessBuilder. ^"[Ljava.lang.String;" (into-array String (list* command args)))]
    (when (contains? opts :directory)
      (.directory pb (:directory opts)))
    (when (contains? opts :env)
      (let [environment (.environment pb)]
        (doseq [[k v] (:env opts)]
          (.put environment k v))))
    (when (contains? opts :redirect-error-stream?)
      (.redirectErrorStream pb (:redirect-error-stream? opts)))
    (.start pb)))

(defn watchdog! ^Thread [^Process proc on-exit]
  (doto (Thread. (fn []
                   (.waitFor proc)
                   (on-exit)))
    (.start)))
