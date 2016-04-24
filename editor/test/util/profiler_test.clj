(ns util.profiler-test
  (:require [clojure.test :refer :all]
            [util.profiler :as profiler]
            [clojure.data.json :as json])
  (:import [com.defold.util Profiler]))

(defn- clear! []
  (Profiler/reset))

(defn- dump []
  (->> (Profiler/dumpJson)
    json/read-str
    (mapv (fn [e] (into {} (map (fn [[k v]] [(keyword k) v]) e))))))

(defn- sleep []
  (try
    (Thread/sleep 1)
    (catch InterruptedException e)))

(deftest threads []
  (clear!)
  (profiler/profile "outer" -1
                    (sleep)
                    (doall
                      (pcalls
                        (fn []
                          (profiler/profile "inner" -1
                                            (sleep))))))
  (let [data (into {} (map (fn [e] [(:name e) e]) (dump)))
        outer (get data "outer")
        inner (get data "inner")]
    (is (not= (:thread outer) (:thread inner)))
    (is (< (:start outer) (:start inner)))
    (is (< (:end inner) (:end outer)))))

(threads)
