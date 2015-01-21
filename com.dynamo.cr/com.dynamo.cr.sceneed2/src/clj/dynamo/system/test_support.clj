(ns dynamo.system.test-support
  (:require [clojure.core.async :as a]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [in]]
            [internal.system :as is]
            [internal.transaction :as it]))

(set! *warn-on-reflection* true)

(defn clean-world
  []
  (let [world-ref (ref nil)
        root      (n/construct n/RootScope :world-ref world-ref :_id 1)]
    (dosync
      (ref-set world-ref (is/new-world-state world-ref root (ref #{})))
      world-ref)))

(defmacro with-clean-world
  [& forms]
  `(let [~'world-ref (clean-world)
         ~'root      (ds/node-by-id ~'world-ref 1)]
     (binding [it/*transaction* (it/->TransactionSeed ~'world-ref)]
       (ds/in ~'root
           ~@forms))))

(defn tx-nodes [& resources]
  (ds/transactional
    (doseq [r resources]
      (ds/add r))
    resources))

(defn await-world-time
  [world-ref desired-time clock-time]
  (let [valch (a/chan 1)
        timer (a/timeout clock-time)]
    (add-watch world-ref :world-time
      (fn [_ _ o n]
        (if (>= (:world-time n) desired-time)
          (a/put! valch n))))
    (first (a/alts!! [valch timer]))))
