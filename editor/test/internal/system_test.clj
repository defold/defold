(ns internal.system-test
  (:require [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [dynamo.graph :as g]
            [internal.graph.dgraph :as dg]
            [internal.system :as is]
            [internal.transaction :as txn]))

(defn- started?     [s] (-> s :world :started?))
(defn- system-graph [s] (-> s :world :state deref :graph))
(defn- graph-root   [s] (dg/node (system-graph s) 1))

(defn fresh-system []  (is/system {:initial-graph (g/project-graph)}))

(deftest lifecycle
  (let [sys (is/start-system (fresh-system))]
    (is (started? sys))
    (is (not (nil? (system-graph sys))))
    (is (not (nil? (:world-ref (graph-root sys)))))
    (is (= 1 (:_id (graph-root sys))))

    (let [sys (is/stop-system sys)
          sys (is/start-system sys)]
      (is (not (nil? (system-graph sys)))))))

(deftest world-time
  (testing "world time advances with transactions"
    (let [sys (is/start-system (fresh-system))
          world-ref (-> sys :world :state)
          root      (graph-root sys)
          before    (:world-time @world-ref)
          tx-report (txn/transact world-ref [(txn/send-message root {:type :noop})])
          after     (:world-time @world-ref)]
      (is (= :ok (:status tx-report)))
      (is (< before after))))
  (testing "world time does *not* advance for empty transactions"
    (let [sys (is/start-system (fresh-system))
          world-ref (-> sys :world :state)
          before    (:world-time @world-ref)
          tx-report (txn/transact world-ref [])
          after     (:world-time @world-ref)]
      (is (= :empty (:status tx-report)))
      (is (= before after)))))
