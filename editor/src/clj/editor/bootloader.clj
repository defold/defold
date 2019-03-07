(ns editor.bootloader
  "Loads namespaces in parallel when possible to speed up start time."
  (:require [clojure.java.io :as io])
  (:import [java.util.concurrent LinkedBlockingQueue]))

(def load-info (atom nil))

(defn load-boot
  []
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
    (reset! load-info [loader boot-loaded?])))

(defn main
  [args]
  ;; Wait until edior.boot is loaded
  ;; TODO: This causes a hiccup in the splash screen animation. Fix?
  (let [[loader ^LinkedBlockingQueue boot-loaded?] @load-info]
    (reset! load-info nil)
    (ns-unmap *ns* 'load-info)
    (.take boot-loaded?)
    (let [boot-main (resolve 'editor.boot/main)]
      (boot-main args loader))))

