(ns internal.system-test
  (:require [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [internal.graph.dgraph :as dg]
            [internal.system :as is]
            [internal.transaction :as txn]))

(defn- started?     [s] (-> s deref :world :started))
(defn- system-graph [s] (-> s deref :world :state deref :graph))
(defn- graph-root   [s] (dg/node (system-graph s) 1))

(deftest lifecycle
  (let [sys (atom (is/system))]
    (is (not (started? sys)))
    (is (nil? (system-graph sys)))

    (is/start sys)
    (is (started? sys))
    (is (not (nil? (system-graph sys))))
    (is (not (nil? (:world-ref (graph-root sys)))))
    (is (= 1 (:_id (graph-root sys))))

    (is/stop sys)
    (is (not (started? sys)))
    (is (nil? (system-graph sys)))

    (is/start sys)
    (is (started? sys))
    (is (not (nil? (system-graph sys))))))

(deftest world-time
  (testing "world time does *not* advance for empty transactions"
    (let [sys (atom (is/system))]
      (is/start sys)
      (let [world-ref (-> sys deref :world :state)
            before    (:world-time @world-ref)
            tx-report (txn/transact world-ref [])
            after     (:world-time @world-ref)]
        (is (= :empty (:status tx-report)))
        (is (= before after)))))
  (testing "world time advances with transactions"
    (let [sys (atom (is/system))]
      (is/start sys)
      (let [world-ref (-> sys deref :world :state)
            root      (graph-root sys)
            before    (:world-time @world-ref)
            tx-report (txn/transact world-ref [(txn/send-message root {:type :noop})])
            after     (:world-time @world-ref)]
        (is (= :ok (:status tx-report)))
        (is (< before after))))))
