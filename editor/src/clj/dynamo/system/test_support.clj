(ns dynamo.system.test-support
  (:require [clojure.core.async :as a]
            [clojure.java.io :as io]
            [com.stuartsierra.component :as component]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [in]]
            [internal.async :as ia]
            [internal.cache :as c]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [internal.transaction :as it]))

(defn clean-system
  []
  (component/start-system
   (is/make-system {:initial-graph (g/project-graph)})))

(defmacro with-clean-system
  [& forms]
  `(let [~'system      (clean-system)
         ~'world       (:world ~'system)
         ~'cache       (:cache ~'system)
         ~'world-ref   (:state ~'world)
         ~'disposal-ch (is/disposal-queue ~'system)
         ~'root        (ds/node ~'world-ref 1)]
     (binding [gt/*transaction* (gt/->TransactionSeed (partial ds/transact ~'system))]
       (ds/in ~'root
              ~@forms))))

(defn tx-nodes [& resources]
  (g/transactional
    (doseq [r resources]
      (g/add r))
    resources))

(defn take-waiting-to-dispose
  [system]
  (ia/take-all (is/disposal-queue system)))

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

(defn yield
  "Give up the thread just long enough for a context switch"
  []
  (Thread/sleep 1))
