(ns editor.process
  (:require [clojure.java.io :as io])
  (:import [java.lang Process ProcessBuilder]))

(set! *warn-on-reflection* true)

(defn start! ^Process [^String command args {:keys [directory env redirect-error-stream?]
                                             :or {redirect-error-stream? false}
                                             :as opts}]
  (let [pb (ProcessBuilder. ^"[Ljava.lang.String;" (into-array String (list* command args)))]
    (when directory
      (.directory pb directory))
    (when env
      (let [environment (.environment pb)]
        (doseq [[k v] env]
          (.put environment k v))))
    (when (some? redirect-error-stream?)
      (.redirectErrorStream pb (boolean redirect-error-stream?)))
    (.start pb)))

(defn watchdog! ^Thread [^Process proc on-exit]
  (doto (Thread. (fn []
                   (.waitFor proc)
                   (on-exit)))
    (.start)))
