(ns editor.bootloader
  "Loads namespaces in parallel when possible to speed up start time."
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io])
  (:import [java.util.concurrent LinkedBlockingQueue]))

(def load-info (atom nil))

(defn load-boot
  []
  (let [batches (->> (io/resource "sorted_clojure_ns_list.edn")
                     (slurp)
                     (edn/read-string))
        boot-loaded? (LinkedBlockingQueue.)
        loader (future
                 (doseq [batch batches]
                   (dorun (pmap require batch))
                   (when (contains? batch 'editor.boot)
                     (.put boot-loaded? (Object.)))))]
    (reset! load-info [loader boot-loaded?])))

(defn wait-until-editor-boot-loaded
  []
  (.take ^LinkedBlockingQueue (second @load-info)))

(defn main
  [args]
  (let [[loader _] @load-info]
    (reset! load-info nil)
    (ns-unmap *ns* 'load-info)
    (let [boot-main (resolve 'editor.boot/main)]
      (boot-main args loader))))

