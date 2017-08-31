(ns editor.handler-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.core :as core]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [service.log :as log])
  (:import [clojure.lang Keyword]))

(defn fixture [f]
  (with-redefs [handler/*handlers* (atom {})]
   (f)))

(use-fixtures :each fixture)

(defn- enabled? [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)]
    (some-> (handler/active command command-contexts user-data)
      handler/enabled?)))

(defn- run [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)]
    (some-> (handler/active command command-contexts user-data)
      handler/run)))

(deftest run-test
  (handler/defhandler :open :global
    (enabled? [instances] (every? #(= % :foo) instances))
    (run [instances] 123))
  (are [inst exp] (= exp (enabled? :open [(handler/->context :global {:instances [inst]})] {}))
       :foo true
       :bar false)
  (is (= 123 (run :open [(handler/->context :global {:instances [:foo]})] {}))))

(deftest context
  (handler/defhandler :c1 :global
    (active? [global-context] true)
    (enabled? [global-context] true)
    (run [global-context]
         (when global-context
           :c1)))
  (handler/defhandler :c2 :local
    (active? [local-context] true)
    (enabled? [local-context] true)
    (run [local-context]
         (when local-context
           :c2)))

  (let [global-context (handler/->context :global {:global-context true})
        local-context (handler/->context :local {:local-context true})]
    (is (enabled? :c1 [local-context global-context] {}))
    (is (not (enabled? :c1 [local-context] {})))
    (is (enabled? :c2 [local-context global-context] {}))
    (is (not (enabled? :c2 [global-context] {})))))

(defrecord StaticSelection [selection]
  handler/SelectionProvider
  (selection [this] selection)
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(defrecord DynamicSelection [selection-ref]
  handler/SelectionProvider
  (selection [this] @selection-ref)
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(extend-type java.lang.String
  core/Adaptable
  (adapt [this t]
    (cond
      (= t Keyword) (keyword this))))

(deftest selection-test
  (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})]
    (handler/defhandler :c1 :global
      (active? [selection] selection)
      (run [selection]
           (handler/adapt selection Keyword)))
    (doseq [[local-selection expected-selection] [[["b"] [:b]]
                                                  [[] []]
                                                  [nil [:a]]]]
      (let [local (handler/->context :local {:local-context true} (when local-selection
                                                                            (->StaticSelection local-selection)) [])]
        (is (= expected-selection (run :c1 [local global] {})))))))

(deftest selection-context
  (let [[global local] (mapv #(handler/->context % {} (->StaticSelection [:a]) {}) [:global :local])]
    (handler/defhandler :c1 :global
      (active? [selection selection-context] (and (= :global selection-context) selection))
      (run [selection]
           nil))
    (is (enabled? :c1 [global] {}))
    (is (not (enabled? :c1 [local] {})))
    (is (enabled? :c1 [local global] {}))))

(deftest erroneous-handler
  (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})]
    (handler/defhandler :erroneous :global
      (active? [does-not-exist] true)
      (enabled? [does-not-exist] true)
      (run [does-not-exist] (throw (Exception. "should never happen"))))
    (log/without-logging
      (is (not (enabled? :erroneous [global] {})))
      (is (nil? (run :erroneous [global] {}))))))

(deftest throwing-handler
  (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})
        throwing-enabled? (test-util/make-call-logger (fn [selection] (throw (Exception. "Thrown from enabled?"))))
        throwing-run (test-util/make-call-logger (fn [selection] (throw (Exception. "Thrown from run"))))]
    (handler/enable-disabled-handlers!)
    (handler/defhandler :throwing :global
      (active? [selection] true)
      (enabled? [selection] (throwing-enabled? selection))
      (run [selection] (throwing-run selection)))
    (log/without-logging
      (testing "The enabled? function will not be called anymore if it threw an exception."
        (is (not (enabled? :throwing [global] {})))
        (is (not (enabled? :throwing [global] {})))
        (is (= 1 (count (test-util/call-logger-calls throwing-enabled?)))))
      (testing "The command can be repeated even though an exception was thrown during run."
        (is (nil? (run :throwing [global] {})))
        (is (nil? (run :throwing [global] {})))
        (is (= 2 (count (test-util/call-logger-calls throwing-run)))))
      (testing "Disabled handlers can be re-enabled during development."
        (is (= 1 (count (test-util/call-logger-calls throwing-enabled?))))
        (enabled? :throwing [global] {})
        (is (= 1 (count (test-util/call-logger-calls throwing-enabled?))))
        (handler/enable-disabled-handlers!)
        (enabled? :throwing [global] {})
        (is (= 2 (count (test-util/call-logger-calls throwing-enabled?))))))))

(defprotocol AProtocol)

(defrecord ARecord []
  AProtocol)

(deftest adaptables
  (is (not-empty (keep identity (handler/adapt [(->ARecord)] AProtocol)))))

(g/defnode StringNode
  (property string g/Str))

(g/defnode IntNode
  (property int g/Int))

(defrecord OtherType [])

(deftest adapt-nodes
  (handler/defhandler :string-command :global
    (enabled? [selection] (handler/adapt-every selection String))
    (run [selection] (handler/adapt-every selection String)))
  (handler/defhandler :single-string-command :global
    (enabled? [selection] (handler/adapt-single selection String))
    (run [selection] (handler/adapt-single selection String)))
  (handler/defhandler :int-command :global
    (enabled? [selection] (handler/adapt-every selection Integer))
    (run [selection] (handler/adapt-every selection Integer)))
  (handler/defhandler :string-node-command :global
    (enabled? [selection] (handler/adapt-every selection StringNode))
    (run [selection] (handler/adapt-every selection StringNode)))
  (handler/defhandler :other-command :global
    (enabled? [selection] (handler/adapt-every selection OtherType))
    (run [selection] (handler/adapt-every selection OtherType)))
  (with-clean-system
    (let [[s i] (tx-nodes (g/make-nodes world
                                        [s [StringNode :string "test"]
                                         i [IntNode :int 1]]))
          selection (atom [])
          select! (fn [s] (reset! selection s))]
      (let [global (handler/->context :global {} (->DynamicSelection selection) {}
                              {String (fn [node-id]
                                         (when (g/node-instance? StringNode node-id)
                                              (g/node-value node-id :string)))
                               Integer (fn [node-id]
                                          (when (g/node-instance? IntNode node-id)
                                               (g/node-value node-id :int)))})]
        (select! [])
        (are [enbl? cmd] (= enbl? (enabled? cmd [global] {}))
             false :string-command
             false :single-string-command
             false :int-command
             false :string-node-command
             false :other-command)
        (select! [s])
        (are [enbl? cmd] (= enbl? (enabled? cmd [global] {}))
             true :string-command
             true :single-string-command
             false :int-command
             true :string-node-command
             false :other-command)
        (select! [s s])
        (are [enbl? cmd] (= enbl? (enabled? cmd [global] {}))
             true :string-command
             false :single-string-command
             false :int-command
             true :string-node-command
             false :other-command)
        (select! [i])
        (are [enbl? cmd] (= enbl? (enabled? cmd [global] {}))
             false :string-command
             false :single-string-command
             true :int-command
             false :string-node-command
             false :other-command)))))

(deftest adapt-nested
  (handler/defhandler :string-node-command :global
    (enabled? [selection] (handler/adapt-every selection StringNode))
    (run [selection] (handler/adapt-every selection StringNode)))
  (with-clean-system
    (let [[s] (tx-nodes (g/make-nodes world
                                      [s [StringNode :string "test"]]))
          selection (atom [])
          select! (fn [s] (reset! selection s))]
      (let [global (handler/->context :global {} (->DynamicSelection selection) {}
                              {Long :node-id})]
        (select! [])
        (is (not (enabled? :string-node-command [global] {})))
        (select! [{:node-id s}])
        (is (enabled? :string-node-command [global] {}))
        (select! [s])
        (is (enabled? :string-node-command [global] {}))
        (select! ["other-type"])
        (is (not (enabled? :string-node-command [global] {})))
        (select! [nil])
        (is (not (enabled? :string-node-command [global] {})))))))

(deftest dynamics
  (handler/defhandler :string-command :global
      (active? [string] string)
      (enabled? [string] string)
      (run [string] string))
  (with-clean-system
    (let [[s] (tx-nodes (g/make-nodes world
                                      [s [StringNode :string "test"]]))]
      (let [global (handler/->context :global {:string-node s} nil {:string [:string-node :string]} {})]
        (is (enabled? :string-command [global] {}))))))

(defn- eval-selection [ctxs all-selections?]
  (get-in (first (handler/eval-contexts ctxs all-selections?)) [:env :selection]))

(defn- eval-selections [ctxs all-selections?]
  (mapv (fn [ctx] (get-in ctx [:env :selection])) (handler/eval-contexts ctxs all-selections?)))

(deftest contexts
  (let [global (handler/->context :global {:selection [0]} nil {} {})]
    (is (= [0] (eval-selection [global] true))))
  (let [global (handler/->context :global {} (StaticSelection. [0]) {} {})]
    (is (= [0] (eval-selection [global] true)))
    (let [local (handler/->context :local {} (StaticSelection. [1]) {} {})]
      (is (= [[1] [1] [0]] (eval-selections [local global] true))))
    (let [local (handler/->context :local {} (StaticSelection. [1]) {} {})]
      (is (= [[1] [1]] (eval-selections [local global] false))))))

(g/defnode ImposterStringNode
  (input source g/NodeID)
  (input string g/Str)
  (output selection-data g/Any (g/fnk [source] (when source {:alt source}))))

(defrecord AltSelection [selection-ref]
  handler/SelectionProvider
  (selection [this] @selection-ref)
  (succeeding-selection [this] [])
  (alt-selection [this] (let [s (handler/selection this)]
                          (if-let [s' (handler/adapt-every s ImposterStringNode)]
                            (->> s'
                              (keep (fn [nid] (:alt (g/node-value nid :selection-data))))
                              vec)
                            []))))

(deftest alternative-selections
  (handler/defhandler :string-command :global
      (active? [selection] (handler/adapt-single selection StringNode))
      (run [selection] (g/node-value (handler/adapt-single selection StringNode) :string)))
  (with-clean-system
    (let [[s i lonely-i] (tx-nodes (g/make-nodes world
                                                 [s [StringNode :string "test"]
                                                  i ImposterStringNode
                                                  lonely-i ImposterStringNode]
                                                 (g/connect s :string i :string)
                                                 (g/connect s :_node-id i :source)))
          selection (atom [])
          select! (fn [s] (reset! selection s))
          selection-provider (->AltSelection selection)
          global (handler/->context :global {} selection-provider {} {})]
      (is (not (enabled? :string-command [global] {})))
      (select! [s])
      (is (enabled? :string-command [global] {}))
      (select! [i])
      (is (enabled? :string-command [global] {}))
      (select! [lonely-i])
      (is (not (enabled? :string-command [global] {}))))))
