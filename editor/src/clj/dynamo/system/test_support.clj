(ns dynamo.system.test-support
  (:require [dynamo.graph :as g]
            [dynamo.system :as ds]
            [internal.async :as ia]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [internal.transaction :as it]))

(defn clean-system
  [configuration]
  (let [configuration (if (:initial-graph configuration)
                        configuration
                        (assoc configuration :initial-graph (g/make-graph [])))]
    (is/start-system (is/make-system configuration))))

(defmacro with-clean-system
  [& forms]
  (let [configuration  (if (map? (first forms)) (first forms) {})
        forms          (if (map? (first forms)) (next forms)  forms)]
    `(let [~'system      (clean-system ~configuration)
           ~'world-ref   (:world ~'system)
           ~'cache       (:cache ~'system)
           ~'disposal-ch (is/disposal-queue ~'system)]
       (binding [ds/*the-system* (atom ~'system)
                 gt/*transaction* (gt/->TransactionSeed (partial ds/transact (atom ~'system)))
                 it/*scope*       nil]
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
