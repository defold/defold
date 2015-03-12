(ns internal.value-test
  (:require [clojure.core.async :as async :refer [chan >!! <! alts!! timeout]]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.system :as is]
            [internal.transaction :as it]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_id node) fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [(:_id node) fn-symbol] 0))

(defmacro expect-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= (inc calls-before#) (get-tally ~node ~fn-symbol)))))

(defmacro expect-no-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= calls-before# (get-tally ~node ~fn-symbol)))))

(defnk produce-simple-value
  [this scalar]
  (tally this 'produce-simple-value)
  scalar)

(g/defnode UncachedOutput
  (property scalar s/Str)
  (output uncached-value String produce-simple-value))

(defn compute-expensive-value
  [node g]
  (tally node 'compute-expensive-value)
  "this took a long time to produce")

(g/defnode CachedOutputNoInputs
  (output expensive-value String :cached
    (fn [node g]
      (tally node 'compute-expensive-value)
      "this took a long time to produce"))
  (input operand String))

(g/defnode UpdatesExpensiveValue
  (output expensive-value String :cached
    (fn [node g]
      (tally node 'compute-expensive-value)
      "this took a long time to produce")))

(g/defnode SecondaryCachedValue
  (output another-value String :cached
    (fn [node g]
      "this is distinct from the other outputs")))

(defnk compute-derived-value
  [this first-name last-name]
  (tally this 'compute-derived-value)
  (str first-name " " last-name))

(defnk passthrough-first-name
  [this first-name]
  (tally this 'passthrough-first-name)
  first-name)

(g/defnode CachedOutputFromInputs
  (input first-name String)
  (input last-name  String)

  (output nickname String :cached passthrough-first-name)
  (output derived-value String :cached compute-derived-value))

(g/defnode CacheTestNode
  (inherits UncachedOutput)
  (inherits CachedOutputNoInputs)
  (inherits CachedOutputFromInputs)
  (inherits UpdatesExpensiveValue)
  (inherits SecondaryCachedValue))

(defn build-sample-project
  []
  (with-clean-system
    (let [nodes (tx-nodes
                  (n/construct CacheTestNode :scalar "Jane")
                  (n/construct CacheTestNode :scalar "Doe")
                  (n/construct CacheTestNode)
                  (n/construct CacheTestNode))
          [name1 name2 combiner expensive]  nodes]
      (g/transactional
        (g/connect name1 :uncached-value combiner :first-name)
        (g/connect name2 :uncached-value combiner :last-name)
        (g/connect name1 :uncached-value expensive :operand))
      [world-ref cache nodes])))

(defn with-function-counts
  [f]
  (binding [*calls* (atom {})]
    (f)))

(use-fixtures :each with-function-counts)

(deftest project-cache
  (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
    (testing "uncached values are unaffected"
      (is (= "Jane" (g/node-value (:graph @world-ref) cache name1 :uncached-value))))))

(deftest caching-avoids-computation
  (testing "cached values are only computed once"
    (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane Doe" (g/node-value (:graph @world-ref) cache combiner :derived-value)))
      (expect-no-call-when combiner 'compute-derived-value
        (doseq [x (range 100)]
          (g/node-value (:graph @world-ref) cache combiner :derived-value)))))

  (testing "modifying inputs invalidates the cached value"
    (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane Doe" (g/node-value (:graph @world-ref) cache combiner :derived-value)))
      (expect-call-when combiner 'compute-derived-value
        (it/transact world-ref [(it/update-property name1 :scalar (constantly "John") [])])
        (is (= "John Doe" (g/node-value (:graph @world-ref) cache combiner :derived-value))))))

  (testing "transmogrifying a node invalidates its cached value"
    (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane Doe" (g/node-value (:graph @world-ref) cache combiner :derived-value)))
      (expect-call-when combiner 'compute-derived-value
        (it/transact world-ref [(it/become name1 (n/construct CacheTestNode))])
        (is (= "Jane Doe" (g/node-value (:graph @world-ref) cache combiner :derived-value))))))

  (testing "cached values are distinct"
    (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "this is distinct from the other outputs" (g/node-value (:graph @world-ref) cache combiner :another-value)))
      (is (not= (g/node-value (:graph @world-ref) cache combiner :another-value) (g/node-value (:graph @world-ref) cache combiner :expensive-value)))))

  (testing "cache invalidation only hits dependent outputs"
    (let [[world-ref cache [name1 name2 combiner expensive]] (build-sample-project)]
      (is (= "Jane" (g/node-value (:graph @world-ref) cache combiner :nickname)))
      (expect-call-when combiner 'passthrough-first-name
        (it/transact world-ref [(it/update-property name1 :scalar (constantly "Mark") [])])
        (is (= "Mark" (g/node-value (:graph @world-ref) cache combiner :nickname))))
      (expect-no-call-when combiner 'passthrough-first-name
        (it/transact world-ref [(it/update-property name2 :scalar (constantly "Brandenburg") [])])
        (is (= "Mark" (g/node-value (:graph @world-ref) cache combiner :nickname)))
        (is (= "Mark Brandenburg" (g/node-value (:graph @world-ref) cache combiner :derived-value)))))))


(defnk compute-disposable-value
  [this g]
  (tally this 'compute-disposable-value)
  (reify t/IDisposable
    (dispose [v]
      (tally this 'dispose)
      (>!! (:channel (dg/node g this)) :gone))))

(g/defnode DisposableValueNode
  (output disposable-value 't/IDisposable :cached compute-disposable-value))

(defnk produce-input-from-node
  [overridden]
  overridden)

(defnk derive-value-from-inputs
  [an-input]
  an-input)

(g/defnode OverrideValueNode
  (input overridden s/Str)
  (output output s/Str produce-input-from-node)
  (output foo    s/Str derive-value-from-inputs))

(defn build-override-project
  []
  (let [nodes (tx-nodes
               (n/construct OverrideValueNode)
               (n/construct CacheTestNode :scalar "Jane"))
        [override jane]  nodes]
    (g/transactional
     (g/connect jane :uncached-value override :overridden))
    nodes))

(deftest invalid-resource-values
  (with-clean-system
    (let [[override jane] (build-override-project)]
      (testing "requesting a non-existent label throws"
        (is (thrown? clojure.lang.ExceptionInfo (g/node-value (:graph @world-ref) cache override :aint-no-thang)))))))

(deftest update-sees-in-transaction-value
  (with-clean-system
    (let [node (g/transactional
                 (ds/add (n/construct p/Project :name "a project" :int-prop 0)))
          after-transaction (g/transactional
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc)
                              (g/update-property node :int-prop inc))]
      (is (= 4 (:int-prop after-transaction))))))

(g/defnode ScopeReceiver
  (on :project-scope
    (g/set-property self :message-received event)))

(deftest node-receives-scope-message
  (testing "project scope message"
    (with-clean-system
      (let [ps-node (g/transactional
                      (ds/in (ds/add (n/construct p/Project :name "a project"))
                        (ds/add (n/construct ScopeReceiver))))]
        (await-world-time world-ref 3 500)
        (is (= "a project" (->> (ds/refresh world-ref ps-node) :message-received :scope :name)))))))
