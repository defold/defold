(ns editor.bootloader
  "Loads namespaces in parallel when possible to speed up start time."
  (:require [clojure.java.io :as io])
  (:import [java.util.concurrent LinkedBlockingQueue]))

(defn load-boot
  [args]
  (let [batches (->> (io/resource "sorted_clojure_ns_list.edn")
                     (slurp)
                     (read-string))
        boot-loaded? (LinkedBlockingQueue.)
        loader (future
                 (let [ns-load-time (System/nanoTime)]
                   (doseq [batch batches]
                     (dorun (pmap require batch))
                     (when (contains? batch 'editor.boot)
                       (.put boot-loaded? (Object.))))
                   (println "****** LOADING NSES TOOK:" (/ (- (System/nanoTime) ns-load-time) 1000000000.0))))]
    ;; Wait until edior.boot is loaded
    (.take boot-loaded?)
    (let [main (resolve 'editor.boot/main)]
      (main args loader))))

