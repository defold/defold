(ns internal.value-test
  (:require [clojure.test :refer :all]
            [dynamo.project.test-support :refer :all]
            [clojure.core.async :as async :refer [chan >!! <! alts!! timeout]]
            [schema.core :as s]
            [dynamo.resource :as r]
            [plumbing.core :refer [defnk]]
            [internal.cache :as c]
            [internal.graph.dgraph :as dg]
            [dynamo.project.test-support :refer :all]
            [dynamo.project :as p]
            [dynamo.node :as n]
            [dynamo.types :as t]))

(defn tx-nodes [project-state & resources]
  (let [tx-result (p/transact project-state (map p/new-resource resources))
        after (:graph tx-result)]
    (map #(dg/node after (p/resolve-tempid tx-result %)) (map :_id resources))))

(defn produce-simple-value
  [node g]
  (tally node 'produce-simple-value)
  (:scalar node))

(n/defnode UncachedOutput
  (property scalar (t/string :default "foo"))

  (output uncached-value String [node g]
          (tally node 'produce-simple-value)
          (:scalar node)))

(defn compute-expensive-value
  [node g]
  (tally node 'compute-expensive-value)
  "this took a long time to produce")

(n/defnode CachedOutputNoInputs
  (output expensive-value :cached [node g]
          (tally node 'compute-expensive-value)
          "this took a long time to produce")
  (input  operand String))

(n/defnode UpdatesExpensiveValue
  (output expensive-value String :cached :on-update
          [node g]
          (tally node 'compute-expensive-value)
          "this took a long time to produce"))

(defnk compute-derived-value
  [this first-name last-name]
  (tally this 'compute-derived-value)
  (str first-name " " last-name))

(n/defnode CachedOutputFromInputs
  (input first-name String)
  (input last-name  String)

  (output derived-value String :cached compute-derived-value))

(n/defnode CacheTestNode
  (inherits UncachedOutput)
  (inherits CachedOutputNoInputs)
  (inherits CachedOutputFromInputs)
  (inherits UpdatesExpensiveValue))

(defn build-sample-project
  []
  (let [nodes (tx-nodes *test-project*
                        (make-cache-test-node :scalar "Jane")
                        (make-cache-test-node :scalar "Doe")
                        (make-cache-test-node)
                        (make-cache-test-node))
        [name1 name2 combiner expensive]  nodes]
    (p/transact *test-project*
                [(p/connect name1 :uncached-value combiner :first-name)
                 (p/connect name2 :uncached-value combiner :last-name)
                 (p/connect name1 :uncached-value expensive :operand)])
    nodes))

(defn with-function-counts
  [f]
  (binding [*calls* (atom {})]
    (f)))

(use-fixtures :each with-clean-project with-function-counts)

(deftest labels-appear-in-cache-keys
  (let [[name1 name2 combiner expensive] (build-sample-project)]
    (testing "uncached values are unaffected"
             (is (contains? (:cache-keys @*test-project*) (:_id combiner)))
             (is (= #{:expensive-value :derived-value} (into #{} (keys (get-in @*test-project* [:cache-keys (:_id combiner)]))))))))

(deftest project-cache
  (let [[name1 name2 combiner expensive] (build-sample-project)]
    (testing "uncached values are unaffected"
             (is (= "Jane" (p/get-resource-value *test-project* name1 :uncached-value))))))

(deftest caching-avoids-computation
  (testing "cached values are only computed once"
           (let [[name1 name2 combiner expensive] (build-sample-project)]
             (is (= "Jane Doe" (p/get-resource-value *test-project* combiner :derived-value)))
             (expect-no-call-when combiner 'compute-derived-value
                                  (doseq [x (range 100)]
                                    (p/get-resource-value *test-project* combiner :derived-value)))))

  (testing "modifying inputs invalidates the cached value"
           (let [[name1 name2 combiner expensive] (build-sample-project)]
             (is (= "Jane Doe" (p/get-resource-value *test-project* combiner :derived-value)))
             (expect-call-when combiner 'compute-derived-value
                               (p/transact *test-project* [(p/update-resource name1 assoc :scalar "John")])
                               (is (= "John Doe" (p/get-resource-value *test-project* combiner :derived-value)))))))


(defnk compute-disposable-value
  [this g]
  (tally this 'compute-disposable-value)
  (reify r/IDisposable
    (dispose [t]
      (tally this 'dispose)
      (>!! (:channel (dg/node g this)) :gone))))

(n/defnode DisposableValueNode
  (output disposable-value :cached compute-disposable-value))

(defnk produce-input-from-node
  [overridden]
  overridden)

(defnk derive-value-from-inputs
  [an-input]
  an-input)

(n/defnode OverrideValueNode
  (input overridden s/Str)
  (output output s/Str produce-input-from-node)
  (output foo    s/Str derive-value-from-inputs))

(defn build-override-project
  []
  (let [nodes (tx-nodes *test-project*
                        (make-override-value-node)
                        (make-cache-test-node :scalar "Jane"))
        [override jane]  nodes]
    (p/transact *test-project*
                [(p/connect jane :uncached-value override :overridden)])
    nodes))

(deftest local-properties
  (let [[override jane]  (build-override-project)]
    (testing "local properties take precedence over wired inputs"
      (is (= "Jane"        (p/get-resource-value *test-project* override :output)))
      (is (= "local value" (p/get-resource-value *test-project* (assoc override :overridden "local value") :output))))
    (testing "local properties are passed to fnks"
      (is (= "value to fnk" (p/get-resource-value *test-project* (assoc override :an-input "value to fnk") :foo))))))

(deftest invalid-resource-values
  (let [[override jane]  (build-override-project)]
    (testing "requesting a non-existent label throws"
      (is (thrown? AssertionError (p/get-resource-value *test-project* override :aint-no-thang))))))
