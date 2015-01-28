(ns internal.refresh-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [fnk]]
            [com.stuartsierra.component :as component]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world]]
            [internal.graph.dgraph :as dg]
            [internal.system :as is]
            [internal.transaction :as txn]))

(defn tally [calls fn-symbol]
  (swap! calls update-in [fn-symbol] (fnil inc 0)))

(defn get-tally [calls fn-symbol]
  (get-in @calls [fn-symbol] 0))

(n/defnode OnUpdateNode
  (input an-input s/Any)

  (property calls s/Any)
  (property a-property s/Any)

  (output input-based-output s/Int :cached :on-update
    (fnk [this calls an-input]
      (tally calls 'input-based-output)
      (when an-input
        (deliver an-input (get-tally calls 'input-based-output)))
      an-input))

  (output property-based-output s/Int :cached :on-update
    (fnk [this calls a-property]
      (tally calls 'property-based-output)
      (when a-property
        (deliver a-property (get-tally calls 'property-based-output)))
      a-property)))

(defn- make-on-update-node [calls]
  (ds/transactional (ds/add (n/construct OnUpdateNode :calls calls :a-property (promise)))))

(n/defnode Driver
  (property latch-property s/Any))

(defn- make-driver [& [connection-latch]]
  (let [n (if connection-latch
            (n/construct Driver :latch-property connection-latch)
            (n/construct Driver))]
    (ds/transactional (ds/add n))))

(defn- started? [s] (-> s deref :world :started))

(deftest value-refresh-after-construction
  (when-not (started? is/the-system)
    (is/start))

  (testing "after construction"
        (let [calls   (atom {})
              tracker (make-on-update-node calls)
              before  (get-tally calls 'property-based-output)]
          (is (= 1 (deref (:a-property tracker) 100 false)))
          #_(is (= (inc before) (get-tally calls 'property-based-output))))))

(deftest value-refresh-after-property-change
  (when-not (started? is/the-system)
    (is/start))

  (testing "after property change"
      (let [calls   (atom {})
            tracker (make-on-update-node calls)]
        (is (deref (:a-property tracker) 100 false))
        (let [before    (get-tally calls 'property-based-output)
              new-latch (promise)]
          (ds/transactional (ds/set-property tracker :a-property new-latch))
          (is (deref new-latch 100 false))
          (is (= (inc before) (get-tally calls 'property-based-output)))))))

(deftest value-refresh-after-input-change
  (when-not (started? is/the-system)
    (is/start))

  (testing "after input change"
    (let [calls            (atom {})
          connection-latch (promise)
          driver           (make-driver connection-latch)
          tracker          (make-on-update-node calls)]
      (ds/transactional (ds/connect driver :latch-property tracker :an-input))
      (is (deref connection-latch 500 false)) ;; Wait for initial refresh due to connection to finish
      (let [before    (get-tally calls 'input-based-output)
            new-latch (promise)]
        (ds/transactional (ds/set-property driver :latch-property new-latch))
        (is (= (inc before) (deref new-latch 100 false)))
        (is (= (inc before) (get-tally calls 'input-based-output)))))))

