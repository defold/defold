;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.transaction-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.transaction :as it]
            [support.test-support :as ts]))

(g/defnk upcase-a [a] (.toUpperCase a))

(g/defnode Resource
  (input a g/Str)
  (output b g/Keyword (g/fnk [] :ok))
  (output c g/Str upcase-a)
  (property d g/Str (default ""))
  (property marker g/Int))

(g/defnode Downstream
  (input consumer g/Keyword)
  (input array-consumer g/Keyword :array))

(defn safe+ [x y] (int (or (and x (+ x y)) y)))

(deftest low-level-transactions
  (testing "one node"
    (ts/with-clean-system
      (let [tx-result (g/transact (g/make-node world Resource :d "known value"))]
        (is (= :ok (:status tx-result))))))
  (testing "two connected nodes"
    (ts/with-clean-system
      (let [[id1 id2] (ts/tx-nodes (g/make-node world Resource)
                                   (g/make-node world Downstream))
            after                 (:basis (g/transact (it/connect id1 :b id2 :consumer)))]
        (is (= [id1 :b]        (first (g/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (g/targets after id1 :b)))))))
  (testing "connections have cardinality"
    (ts/with-clean-system
      (let [[id1 id2] (ts/tx-nodes (g/make-node world Resource)
                                   (g/make-node world Downstream))]
        (g/transact (g/connect id1 :b id2 :array-consumer))
        (g/transact (g/connect id1 :b id2 :array-consumer))
        (is (= 2 (count (g/sources-of id2 :array-consumer))))
        (is (= 2 (count (g/inputs id2)))))))
  (testing "disconnect disconnects all matching"
    (ts/with-clean-system
      (let [[id1 id2] (ts/tx-nodes (g/make-node world Resource)
                                   (g/make-node world Downstream))]
        (g/transact (g/connect id1 :b id2 :array-consumer))
        (g/transact (g/connect id1 :b id2 :array-consumer))
        (g/transact (g/disconnect id1 :b id2 :array-consumer))
        (is (= 0 (count (g/sources-of id2 :array-consumer))))
        (is (= 0 (count (g/inputs id2)))))))
  (testing "disconnect two singly-connected nodes"
    (ts/with-clean-system
      (let [[id1 id2] (ts/tx-nodes (g/make-node world Resource)
                                   (g/make-node world Downstream))
            tx-result             (g/transact (it/connect    id1 :b id2 :consumer))
            tx-result             (g/transact (it/disconnect id1 :b id2 :consumer))
            after                 (:basis tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= [] (g/sources after id2 :consumer)))
        (is (= [] (g/targets after id1 :b))))))

  (testing "simple update"
    (ts/with-clean-system
      (let [[resource] (ts/tx-nodes (g/make-node world Resource :marker (int 0)))
            tx-result  (g/transact (it/update-property resource :marker safe+ [42] nil))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (g/node-value resource :marker))))))

  (testing "node deletion"
    (ts/with-clean-system
      (let [[resource1 resource2] (ts/tx-nodes (g/make-node world Resource)
                                               (g/make-node world Downstream))]
        (g/transact (it/connect resource1 :b resource2 :consumer))
        (let [tx-result  (g/transact (it/delete-node resource2))
              after      (:basis tx-result)]
          (is (nil?      (g/node-by-id   after resource2)))
          (is (empty?    (g/targets      after resource1 :b)))
          (is (contains? (:nodes-deleted tx-result) resource2))
          (is (empty?    (:nodes-added   tx-result)))))))

  (testing "node deletion in same transaction"
    (ts/with-clean-system
      (let [tx-result (g/transact
                       (g/make-nodes world
                                     [resource Resource]
                                     (g/delete-node resource)))
            [node]    (g/tx-nodes-added tx-result)]
        (is (= :ok (:status tx-result)))
        (is (nil?  node))))))

(g/defnode NamedThing
  (property name g/Str))

(g/deftype Date java.util.Date)

(g/defnode Person
  (property date-of-birth Date)

  (input first-name g/Str)
  (input surname g/Str)

  (output friendly-name g/Str (g/fnk [first-name] first-name))
  (output full-name g/Str (g/fnk [first-name surname] (str first-name " " surname)))
  (output age Date (g/fnk [date-of-birth] date-of-birth)))

(g/defnode Receiver
  (input generic-input g/Any)
  (property touched g/Bool (default false))
  (output passthrough g/Any (g/fnk [generic-input] generic-input)))

(g/defnode FocalNode
  (input aggregator g/Any :array)
  (output aggregated [g/Any] (g/fnk [aggregator] aggregator)))

(defn- build-network
  [world]
  (let [nodes {:person            (g/make-node world Person)
               :first-name-cell   (g/make-node world NamedThing)
               :last-name-cell    (g/make-node world NamedThing)
               :greeter           (g/make-node world Receiver)
               :formal-greeter    (g/make-node world Receiver)
               :calculator        (g/make-node world Receiver)
               :multi-node-target (g/make-node world FocalNode)}
        nodes (zipmap (keys nodes) (apply ts/tx-nodes (vals nodes)))]
    (g/transact
     (for [[from from-l to to-l]
           [[:first-name-cell :name          :person            :first-name]
            [:last-name-cell  :name          :person            :surname]
            [:person          :friendly-name :greeter           :generic-input]
            [:person          :full-name     :formal-greeter    :generic-input]
            [:person          :age           :calculator        :generic-input]
            [:person          :full-name     :multi-node-target :aggregator]
            [:formal-greeter  :passthrough   :multi-node-target :aggregator]]]
       (g/connect (from nodes) from-l (to nodes) to-l)))
    nodes))

(defmacro affected-by [& forms]
  `(let [tx-result# (g/transact ~@forms)]
     (set (:outputs-modified tx-result#))))

(defn pairwise [m]
  (for [[k vs] m
        v vs]
    (gt/endpoint k v)))

(deftest precise-invalidation
  (ts/with-clean-system
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network world)]
      (are [update expected] (= (into #{} (pairwise expected)) (affected-by (apply g/set-properties update)))
        [calculator :touched true]                {calculator        #{:_declared-properties :_properties :touched}}
        [person :date-of-birth (java.util.Date.)] {person            #{:_declared-properties :_properties :age :date-of-birth}
                                                   calculator        #{:passthrough}}
        [first-name-cell :name "Sam"]             {first-name-cell   #{:_declared-properties :_properties :name}
                                                   person            #{:full-name :friendly-name}
                                                   greeter           #{:passthrough}
                                                   formal-greeter    #{:passthrough}
                                                   multi-node-target #{:aggregated}}))))


(deftest blanket-invalidation
  (ts/with-clean-system
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network world)
          tx-result        (g/transact (g/invalidate person))
          outputs-modified (:outputs-modified tx-result)]
      (doseq [output [:_node-id :_properties :friendly-name :full-name :date-of-birth :age]]
        (is (some #{(g/endpoint person output)} outputs-modified))))))


(g/defnode CachedOutputInvalidation
  (property a-property g/Str (default "a-string"))

  (output ordinary g/Str :cached (g/fnk [a-property] a-property))
  (output self-dependent g/Str :cached (g/fnk [ordinary] ordinary)))

(deftest invalidated-properties-noted-by-transaction
  (ts/with-clean-system
    (let [tx-result        (g/transact (g/make-node world CachedOutputInvalidation))
          real-id          (first (g/tx-nodes-added tx-result))
          outputs-modified (:outputs-modified tx-result)]
      (is (some #{real-id} (map gt/endpoint-node-id outputs-modified)))
      (is (= #{:_declared-properties :_properties :_overridden-properties :_node-id :_output-jammers :self-dependent :a-property :ordinary}
             (into #{} (map gt/endpoint-label) outputs-modified)))
      (let [tx-data          [(it/update-property real-id :a-property (constantly "new-value") [] nil)]
            tx-result        (g/transact tx-data)
            outputs-modified (:outputs-modified tx-result)]
        (is (some #{real-id} (map gt/endpoint-node-id outputs-modified)))
        (is (= #{:_declared-properties :_properties :a-property :ordinary :self-dependent}
               (into #{} (map gt/endpoint-label) outputs-modified)))))))

(g/defnode CachedValueNode
  (output cached-output g/Str :cached (g/fnk [] "an-output-value")))

(defn cache-peek
  [node-id output]
  (get (g/cache) (gt/endpoint node-id output)))

;; TODO - move this to an integration test group
(deftest values-of-a-deleted-node-are-removed-from-cache
  (ts/with-clean-system
    (let [[node-id]  (ts/tx-nodes (g/make-node world CachedValueNode))]
      (is (= "an-output-value" (g/node-value node-id :cached-output)))
      (let [cached-value (cache-peek node-id :cached-output)]
        (is (= "an-output-value" cached-value))
        (g/transact (g/delete-node node-id))
        (is (nil? (cache-peek node-id :cached-output)))))))

(g/defnode Container
  (input nodes g/Any :array :cascade-delete))

(deftest double-deletion-is-safe
  (testing "delete scope first"
    (ts/with-clean-system
      (let [[outer inner] (ts/tx-nodes (g/make-node world Container) (g/make-node world Resource))]
        (g/transact (g/connect inner :_node-id outer :nodes))
        (is (= :ok (:status (g/transact
                             (concat
                              (g/delete-node outer)
                              (g/delete-node inner)))))))))

  (testing "delete inner node first"
    (ts/with-clean-system
      (let [[outer inner] (ts/tx-nodes (g/make-node world Container) (g/make-node world Resource))]
        (g/transact (g/connect inner :_node-id outer :nodes))

        (is (= :ok (:status (g/transact
                             (concat
                              (g/delete-node inner)
                              (g/delete-node outer))))))))))

(defn- exists? [node-id] (not (nil? (g/node-by-id (g/now) node-id))))

(g/defnode CascadingContainer
  (property a-property g/Str (default ""))
  (input attachments g/Any :cascade-delete :array))

(deftest cascading-delete
  (testing "delete container, one cascade connected by one output"
    (ts/with-clean-system
      (let [[container resource] (ts/tx-nodes (g/make-node world CascadingContainer) (g/make-node world Resource))]
        (g/transact
         (g/connect resource :b container :attachments))
        (is (= :ok (:status (g/transact (g/delete-node container)))))
        (is (not (exists? container)))
        (is (not (exists? resource))))))

  (testing "delete container, one cascade connected by multiple outputs"
    (ts/with-clean-system
      (let [[container resource] (ts/tx-nodes (g/make-node world CascadingContainer) (g/make-node world Resource))]
        (g/transact
         [(g/connect resource :b container :attachments)
          (g/connect resource :c container :attachments)])
        (is (= :ok (:status (g/transact (g/delete-node container)))))
        (is (not (exists? container)))
        (is (not (exists? resource))))))

  (testing "delete container, one cascade connected by a property"
    (ts/with-clean-system
      (let [[container resource] (ts/tx-nodes (g/make-node world CascadingContainer) (g/make-node world Resource))]
        (g/transact
         (g/connect resource :d container :attachments))
        (is (= :ok (:status (g/transact (g/delete-node container)))))
        (is (not (exists? container)))
        (is (not (exists? resource))))))

  (testing "delete container, two cascades"
    (ts/with-clean-system
      (let [[container resource1 resource2] (ts/tx-nodes (g/make-node world CascadingContainer)
                                                         (g/make-node world Resource)
                                                         (g/make-node world Resource))]
        (g/transact
         [(g/connect resource1 :d container :attachments)
          (g/connect resource2 :d container :attachments)])
        (is (= :ok (:status (g/transact (g/delete-node container)))))
        (is (not (exists? container)))
        (is (not (exists? resource1)))
        (is (not (exists? resource2))))))

  (testing "delete container, daisy chain of deletes"
    (ts/with-clean-system
      (let [[container middle1 middle2 resource] (ts/tx-nodes (g/make-node world CascadingContainer)
                                                              (g/make-node world CascadingContainer)
                                                              (g/make-node world CascadingContainer)
                                                              (g/make-node world Resource))]
        (g/transact
         [(g/connect resource  :d          middle2   :attachments)
          (g/connect middle2   :a-property middle1   :attachments)
          (g/connect middle1   :a-property container :attachments)])
        (is (= :ok (:status (g/transact (g/delete-node container)))))
        (is (not (exists? container)))
        (is (not (exists? middle1)))
        (is (not (exists? middle2)))
        (is (not (exists? resource)))))))

(g/defnode PropSource
  (output label g/Keyword (g/fnk [] :label)))

(g/defnode PropTarget
  (property target g/Keyword
            (value (g/fnk [label] (println :target :value-fn label) label))
            (set (fn [evaluation-context self _ new-value]
                   (when-let [src (g/node-value self :source-id evaluation-context)]
                     (println :target :connecting src new-value :to self :label)
                     (g/connect src new-value self :label)))))
  (property second g/Keyword
            (set (fn [evaluation-context self old-value new-value]
                   (println :second :new-value new-value)
                   (when-let [t (g/node-value self :target evaluation-context)]
                     (println :second :t t)
                     (g/set-property self :second t)))))
  (property third g/Keyword
            (set (fn [evaluation-context self old-value new-value]
                   (when-let [t (g/node-value self :implicit-target evaluation-context)]
                     (g/set-property self :third t)))))
  (input source-id g/NodeID)
  (input label g/Keyword)
  (output implicit-target g/Keyword (g/fnk [target] target)))

;;; RAGNAR - the order of initialization on properties is not
;;; guaranteed. This test needs some rewrites.

#_(deftest property-dependencies
    (ts/with-clean-system
      (let [[source target] (ts/tx-nodes (g/make-nodes world [source PropSource
                                                              target [PropTarget :target :label :second :ignored :third :ignored]]
                                                       (g/connect source :_node-id target :source-id)))]
        (is (= :label (g/node-value target :target)))
        (is (= :label (g/node-value target :second)))
        (is (= :label (g/node-value target :third))))))

(g/defnode MultiInput
  (input in g/Keyword :array))

(deftest node-deletion-pull-input
  (ts/with-clean-system
    (let [view-graph (g/make-graph! :volatility 1)
          [src-node] (g/tx-nodes-added
                      (g/transact
                       (g/make-nodes world
                                     [resource Resource])))
          [tgt-node] (g/tx-nodes-added
                      (g/transact
                       (g/make-nodes view-graph
                                     [view MultiInput]
                                     (g/connect src-node :b view :in))))]
      (is (= [:ok] (g/node-value tgt-node :in)))
      (g/delete-node! src-node)
      (is (= [] (g/node-value tgt-node :in))))))
