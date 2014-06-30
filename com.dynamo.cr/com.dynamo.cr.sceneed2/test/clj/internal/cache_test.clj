(ns internal.cache-test
  (:require [clojure.test :refer :all]
            [clojure.core.async :as async :refer [chan >!! <! alts!! timeout]]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [internal.cache :as c]
            [internal.graph.dgraph :as dg]
            [dynamo.project :as p]
            [dynamo.node :as n]))

(def ^:private ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [node fn-symbol] (fnil inc 0)))

(defn get-tally [resource fn-symbol]
  (get-in @*calls* [(:_id resource) fn-symbol]))

(defn tx-nodes [& resources]
  (let [tx-result (p/transact (map p/new-resource resources))
        after (:graph tx-result)]
    (map #(dg/node after (p/resolve-tempid tx-result %)) (map :_id resources))))

(defn produce-simple-value
  [node g]
  (tally node 'produce-simple-value)
  (:scalar (dg/node g node)))

(def UncachedOutput
  {:transforms {:uncached-value #'produce-simple-value}
   :properties {:scalar (n/string :default "foo")}})

(defn compute-expensive-value
  [node g]
  (tally node 'compute-expensive-value)
  "this took a long time to produce")

(def CachedOutputNoInputs
  {:transforms {:expensive-value #'compute-expensive-value}
   :cached #{:expensive-value}})

(defnk compute-derived-value
  [this first-name last-name]
  (tally this 'compute-derived-value)
  (str first-name " " last-name))

(def CachedOutputFromInputs
  {:inputs {:first-name s/Str
            :last-name s/Str}
   :transforms {:derived-value #'compute-derived-value}
   :cached #{:derived-value}})

(n/defnode CacheTestNode
  UncachedOutput
  CachedOutputNoInputs
  CachedOutputFromInputs)

(defn build-sample-project
  []
  (let [nodes (tx-nodes (make-cache-test-node :scalar "Jane") (make-cache-test-node :scalar "Doe")
                        (make-cache-test-node) (make-cache-test-node))
        [name1 name2 combiner & _]  nodes]
    (p/transact [(p/connect name1 :uncached-value combiner :first-name) (p/connect name2 :uncached-value combiner :last-name)])
    nodes))


(defn with-function-counts
  [f]
  (binding [*calls* (atom {})]
    (f)))

(defn with-clean-project
  [f]
  (dosync
    (ref-set p/project-state (p/make-project)))
  (f))

(use-fixtures :each with-clean-project with-function-counts)

(deftest labels-appear-in-cache-keys
  (let [[name1 name2 combiner expensive] (build-sample-project)]
    (testing "uncached values are unaffected"
             (is (contains? (:cache-keys @p/project-state) (:_id combiner)))
             (is (= #{:expensive-value :derived-value} (into #{} (keys (get-in @p/project-state [:cache-keys (:_id combiner)]))))))))

(deftest project-cache
  (let [[name1 name2 combiner expensive] (build-sample-project)]
    (testing "uncached values are unaffected"
             (is (= "Jane" (p/get-resource-value name1 :uncached-value))))))

(deftest caching-avoids-computation
  (testing "cached values are only computed once"
           (let [[name1 name2 combiner expensive] (build-sample-project)]
             (is (= "Jane Doe" (p/get-resource-value combiner :derived-value)))
             (is (= 1 (get-tally combiner 'compute-derived-value)))
             (doseq [x (range 100)]
               (p/get-resource-value combiner :derived-value))
             (is (= 1 (get-tally combiner 'compute-derived-value)))))
  (testing "modifying inputs invalidates the cached value"
           (let [[name1 name2 combiner expensive] (build-sample-project)]
             (is (= "Jane Doe" (p/get-resource-value combiner :derived-value)))
             (is (= 1 (get-tally combiner 'compute-derived-value)))
             (p/transact [(p/update-resource name1 assoc :scalar "John")])
             (is (= "John Doe" (p/get-resource-value combiner :derived-value)))
             (is (= 2 (get-tally combiner 'compute-derived-value))))))


(defnk compute-disposable-value
  [this g]
  (tally this 'compute-disposable-value)
  (reify p/IDisposable
    (dispose [t]
      (tally this 'dispose)
      (>!! (:channel (dg/node g this)) :gone))))

(def CachedDisposableValue
  {:transforms {:disposable-value #'compute-disposable-value}
   :cached #{:disposable-value}})

(n/defnode DisposableValueNode
  CachedDisposableValue)


(deftest disposable-values-are-disposed
  (let [disposal-notices  (chan 1)
        [disposable-node] (tx-nodes (assoc (make-disposable-value-node) :channel disposal-notices))
        val               (p/get-resource-value disposable-node :disposable-value)]
    (testing "node supplies initial value"
             (is (not (nil? val)))
             (is (= 1 (get-tally disposable-node 'compute-disposable-value))))
    (testing "modifying the node causes the value to be discarded"
             (p/transact [(p/update-resource disposable-node assoc :any-attribute "any old value")]))
    (let [[v ch] (alts!! [(timeout 50) disposal-notices])]
      (is (not (nil? v)) "Timeout elapsed before .dispose was called.")
      (is (= v :gone))
      (is (= 1 (get-tally disposable-node 'dispose)))
      (testing "the node can recreate the value on demand."
               (is (not (nil? (p/get-resource-value disposable-node :disposable-value))))))))