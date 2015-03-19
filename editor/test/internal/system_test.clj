(ns internal.system-test
  (:require [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [dynamo.graph :as g]
            [dynamo.system :as ds]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.transaction :as it]))

(defn- started?     [s] (-> s :world :started?))
(defn- graph-root   [s] (ig/node (is/world-graph s) 1))

(defn fresh-system []
  (-> {:initial-graph (g/project-graph)}
      is/make-system
      is/start-system))

(deftest lifecycle
  (let [sys (is/start-system (fresh-system))]
    (is (started? sys))
    (is (not (nil? (is/world-graph sys))))
    (is (not (nil? (:world-ref (graph-root sys)))))
    (is (= 1 (:_id (graph-root sys))))
    (let [sys (is/stop-system sys)
          sys (is/start-system sys)]
      (is (not (nil? (is/world-graph sys)))))))

(deftest world-time
  (testing "world time advances with transactions"
    (let [sys (is/start-system (fresh-system))
          world-ref (is/world-ref sys)
          root      (graph-root sys)
          before    (is/world-time sys)
          tx-report (ds/transact sys [(it/set-property (graph-root sys) :updated true)])
          after     (is/world-time sys)]
      (is (= :ok (:status tx-report)))
      (is (< before after))))
  (testing "world time does *not* advance for empty transactions"
    (let [sys (is/start-system (fresh-system))
          world-ref (is/world-ref sys)
          before    (is/world-time sys)
          tx-report (ds/transact sys [])
          after     (is/world-time sys)]
      (is (= :empty (:status tx-report)))
      (is (= before after)))))
