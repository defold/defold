(ns internal.system-test
  (:require [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :as ts]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.transaction :as it]))

(g/defnode Root)

(defn- started?     [s] (-> s :world :started?))

(defn fresh-system [] (ts/clean-system))

(deftest lifecycle
  (let [sys (fresh-system)]
    (is (started? sys))
    (is (not (nil? (is/world-graph sys))))
    (let [sys (is/stop-system sys)
          sys (is/start-system sys)]
      (is (not (nil? (is/world-graph sys)))))))

(deftest tx-id
  (testing "world time advances with transactions"
    (let [sys (fresh-system)
          world-ref (is/world-ref sys)
          root      (n/construct Root)
          before    (is/world-time sys)
          tx-report (ds/transact (atom sys) [(it/new-node root)])
          after     (is/world-time sys)]
      (is (= :ok (:status tx-report)))
      (is (< before after))))

  (testing "world time does *not* advance for empty transactions"
    (let [sys (fresh-system)
          world-ref (is/world-ref sys)
          before    (is/world-time sys)
          tx-report (ds/transact (atom sys) [])
          after     (is/world-time sys)]
      (is (= :empty (:status tx-report)))
      (is (= before after)))))
