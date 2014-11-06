(ns internal.value-test
  (:require [clojure.test :refer :all]
            [clojure.core.async :as async :refer [chan >!! <! alts!! timeout]]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n :refer [Scope]]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.system :as is]
            [internal.transaction :as it]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_id node) fn-symbol] (fnil inc 0)))

(defn get-tally [resource fn-symbol]
  (get-in @*calls* [(:_id resource) fn-symbol] 0))

(defmacro expect-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= (inc calls-before#) (get-tally ~node ~fn-symbol)))))

(defmacro expect-no-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= calls-before# (get-tally ~node ~fn-symbol)))))

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
  (output expensive-value String :cached [node g]
    (tally node 'compute-expensive-value)
    "this took a long time to produce")
  (input  operand String))

(n/defnode UpdatesExpensiveValue
  (output expensive-value String :cached :on-update
    [node g]
    (tally node 'compute-expensive-value)
    "this took a long time to produce"))

(n/defnode SecondaryCachedValue
  (output another-value String :cached
    [node g]
    "this is distinct from the other outputs"))

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
  (inherits UpdatesExpensiveValue)
  (inherits SecondaryCachedValue))

(defn build-sample-project
  []
  (with-clean-world
    (let [nodes (tx-nodes
                  (make-cache-test-node :scalar "Jane")
                  (make-cache-test-node :scalar "Doe")
                  (make-cache-test-node)
                  (make-cache-test-node))
          [name1 name2 combiner expensive]  nodes]
      (ds/transactional
        (ds/connect name1 :uncached-value combiner :first-name)
        (ds/connect name2 :uncached-value combiner :last-name)
        (ds/connect name1 :uncached-value expensive :operand))
      [world-ref nodes])))

(defn with-function-counts
  [f]
  (binding [*calls* (atom {})]
    (f)))

(use-fixtures :each with-function-counts)

(deftest labels-appear-in-cache-keys
  (let [[world-ref [name1 name2 combiner expensive]] (build-sample-project)]
    (testing "uncached values are unaffected"
      (is (contains? (:cache-keys @world-ref) (:_id combiner)))
      (is (= #{:expensive-value :derived-value :another-value}
            (into #{} (keys (get-in @world-ref [:cache-keys (:_id combiner)]))))))))

(deftest project-cache
  (let [[world-ref [name1 name2 combiner expensive]] (build-sample-project)]
    (testing "uncached values are unaffected"
      (is (= "Jane" (n/get-node-value name1 :uncached-value))))))

(deftest caching-avoids-computation
  (testing "cached values are only computed once"
    (let [[world-ref [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane Doe" (n/get-node-value combiner :derived-value)))
      (expect-no-call-when combiner 'compute-derived-value
        (doseq [x (range 100)]
          (n/get-node-value combiner :derived-value)))))

  (testing "modifying inputs invalidates the cached value"
    (let [[world-ref [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane Doe" (n/get-node-value combiner :derived-value)))
      (expect-call-when combiner 'compute-derived-value
        (it/transact world-ref [(it/update-node name1 assoc :scalar "John")])
        (is (= "John Doe" (n/get-node-value combiner :derived-value))))))

  (testing "cached values are distinct"
    (let [[world-ref [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "this is distinct from the other outputs" (n/get-node-value combiner :another-value)))
      (is (not= (n/get-node-value combiner :another-value) (n/get-node-value combiner :expensive-value))))))


(defnk compute-disposable-value
  [this g]
  (tally this 'compute-disposable-value)
  (reify t/IDisposable
    (dispose [v]
      (tally this 'dispose)
      (>!! (:channel (dg/node g this)) :gone))))

(n/defnode DisposableValueNode
  (output disposable-value t/IDisposable :cached compute-disposable-value))

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
  (with-clean-world
    (let [nodes (tx-nodes
                  (make-override-value-node)
                  (make-cache-test-node :scalar "Jane"))
          [override jane]  nodes]
      (ds/transactional
        (ds/connect jane :uncached-value override :overridden))
      nodes)))

(deftest local-properties
  (let [[override jane]  (build-override-project)]
    (testing "local properties take precedence over wired inputs"
      (is (= "Jane"        (n/get-node-value override :output)))
      (is (= "local value" (n/get-node-value (assoc override :overridden "local value") :output))))
    (testing "local properties are passed to fnks"
      (is (= "value to fnk" (n/get-node-value (assoc override :an-input "value to fnk") :foo))))))

(deftest invalid-resource-values
  (let [[override jane]  (build-override-project)]
    (testing "requesting a non-existent label throws"
      (is (thrown? AssertionError (n/get-node-value override :aint-no-thang))))))

(defnk produce-output-with-project
  [project]
  (:name project))

(n/defnode ProjectAwareNode
  (inherits Scope)
  (output testable-output s/Str produce-output-with-project))

(defn build-project-aware-node
  [project-name]
  (with-clean-world
    (ds/transactional
      (ds/in (ds/add (p/make-project :name project-name))
        (ds/in (ds/add (make-project-aware-node))
          (ds/add (make-project-aware-node)))))))

(deftest sends-node-project-to-production-function
  (let [project-aware-node (build-project-aware-node :some-project)]
    (is (= :some-project (n/get-node-value project-aware-node :testable-output)))))

(deftest update-node-sees-in-transaction-value
  (with-clean-world
    (let [node (ds/transactional
                 (ds/add (p/make-project :name "a project" :int-prop 0)))
          after-transaction (ds/transactional
                              (ds/update-property node :int-prop inc)
                              (ds/update-property node :int-prop inc)
                              (ds/update-property node :int-prop inc)
                              (ds/update-property node :int-prop inc)
                              node)]
      (is (= 4 (:int-prop after-transaction))))))

(n/defnode ScopeReceiver
  (on :project-scope
    (ds/set-property self :message-received event)))

(deftest node-receives-scope-message
  (testing "project scope message"
    (with-clean-world
      (let [ps-node (ds/transactional
                      (ds/in (ds/add (p/make-project :name "a project"))
                        (ds/add (make-scope-receiver))))]
        (await-world-time world-ref 3 500)
        (is (= "a project" (->> (ds/refresh world-ref ps-node) :message-received :scope :name)))))))
