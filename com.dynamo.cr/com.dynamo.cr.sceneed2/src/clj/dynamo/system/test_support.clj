(ns dynamo.system.test-support
  (:require [clojure.core.async :as a]
            [clojure.java.io :as io]
            [com.stuartsierra.component :as component]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [in]]
            [internal.system :as is]
            [internal.transaction :as it]))

(defn clean-world
  []
  (let [report-ch (a/chan (a/dropping-buffer 1))
        world     (is/world report-ch (ref #{}))]
    (component/start world)))

(defmacro with-clean-world
  [& forms]
  `(let [~'world     (clean-world)
         ~'world-ref (:state ~'world)
         ~'root      (ds/node ~'world-ref 1)]
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

(defn tempfile
  ^java.io.File [prefix suffix auto-delete?]
  (let [f (java.io.File/createTempFile prefix suffix)]
    (when auto-delete?
      (.deleteOnExit f))
    f))

(defn array= [a b]
  (and
    (= (class a) (class b))
    (= (count a) (count b))
    (every? true? (map = a b))))
