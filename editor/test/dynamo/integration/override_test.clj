;; Copyright 2020-2026 The Defold Foundation
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

(ns dynamo.integration.override-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.integration.override-test-support :as support]
            [editor.util :as util]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [internal.util]
            [schema.core :as s]
            [support.test-support :as ts]
            [util.fn :as fn])
  (:import [javax.vecmath Vector3d]))

(g/defnode BaseNode
  (property base-property g/Str))

(g/defnode MainNode
  (inherits BaseNode)
  (property a-property g/Str)
  (property b-property g/Str)
  (property virt-property g/Str
            (value (g/fnk [a-property] a-property)))
  (property dyn-property g/Str
            (dynamic override? (g/fnk [_this] (g/node-override? _this))))
  (input sub-nodes g/NodeID :array :cascade-delete)
  (output sub-nodes [g/NodeID] (g/fnk [sub-nodes] sub-nodes))
  (output cached-output g/Str :cached (g/fnk [a-property] a-property))
  (input cached-values g/Str :array)
  (output cached-values [g/Str] :cached (g/fnk [cached-values] cached-values))
  (output _properties g/Properties (g/fnk [_declared-properties]
                                     (-> _declared-properties
                                         (update :properties assoc :c-property (-> _declared-properties :properties :a-property))
                                         (update :display-order conj :c-property)))))

(g/defnode SubNode
  (property value g/Str)
  (input sub-nodes g/NodeID :array :cascade-delete)
  (output sub-nodes [g/NodeID] (g/fnk [sub-nodes] sub-nodes)))

(defn- override [node-id]
  (-> (g/override node-id {})
      ts/tx-nodes))

(defn- setup
  ([world]
   (setup world 0))
  ([world count]
   (let [nodes (ts/tx-nodes (g/make-nodes world [main [MainNode :a-property "main" :b-property "main"]
                                                 sub SubNode]
                              (g/connect sub :_node-id main :sub-nodes)))]
     (loop [result [nodes]
            counter count]
       (if (> counter 0)
         (recur (conj result (override (first (last result)))) (dec counter))
         result)))))

(deftest test-sanity
  (ts/with-clean-system
    (let [[[main sub]] (setup world)]
      (testing "Original graph connections"
        (is (= [sub] (g/node-value main :sub-nodes)))))))

(deftest default-behaviour
  (ts/with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Connection is overridden from start"
        (is (= [or-sub] (g/node-value or-main :sub-nodes)))
        (is (= or-main (g/node-value or-main :_node-id))))
      (testing "Using base values"
        (doseq [label [:a-property :b-property :virt-property :cached-output]]
          (is (= "main" (g/node-value or-main label)))))
      (testing "Base node invalidates cache"
        (g/transact (g/set-property main :a-property "new main"))
        (is (= "new main" (g/node-value or-main :cached-output)))))))

(deftest property-dynamics
  (ts/with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Property dynamics"
        (let [f (fn [n]
                  (let [p (g/node-value n :_declared-properties)]
                    (get-in p [:properties :dyn-property :override?])))]
          (is (false? (f main)))
          (is (true? (f or-main))))))))

(deftest connect
  (testing "Connecting to original spawns override nodes in direct override layers"
    (ts/with-clean-system
      ;; Test with two direct overrides of main.
      (let [[main] (ts/tx-nodes (g/make-nodes world [main MainNode]
                                  (g/override main)
                                  (g/override main)))
            [new1-sub new2-sub] (ts/tx-nodes (g/make-nodes world [new1-sub SubNode
                                                                  new2-sub SubNode
                                                                  new2-sub-sub SubNode]
                                               (g/connect new2-sub-sub :_node-id new2-sub :sub-nodes)))
            node-count-before-connection (-> world g/graph :nodes count)]
        (g/connect! new1-sub :_node-id main :sub-nodes)
        (is (= (+ node-count-before-connection 2) ; Single node cloned to two override layers.
               (-> world g/graph :nodes count)))
        (g/connect! new2-sub :_node-id main :sub-nodes)
        (is (= (+ node-count-before-connection 2 4) ; Two-node chain cloned to two override layers.
               (-> world g/graph :nodes count))))))
  (testing "Connecting to original spawns override nodes in recursive override layers"
    (ts/with-clean-system
      ;; Test with one direct override of main, and one override of that override.
      (let [[main] (ts/tx-nodes (g/make-node world MainNode))
            [or-main] (ts/tx-nodes (g/override main))
            [_or2-main] (ts/tx-nodes (g/override or-main))
            [new1-sub new2-sub] (ts/tx-nodes (g/make-nodes world [new1-sub SubNode
                                                                  new2-sub SubNode
                                                                  new2-sub-sub SubNode]
                                               (g/connect new2-sub-sub :_node-id new2-sub :sub-nodes)))
            node-count-before-connection (-> world g/graph :nodes count)]
        (g/connect! new1-sub :_node-id main :sub-nodes)
        (is (= (+ node-count-before-connection 2) ; Single node cloned to two override layers.
               (-> world g/graph :nodes count)))
        (g/connect! new2-sub :_node-id main :sub-nodes)
        (is (= (+ node-count-before-connection 2 4) ; Two-node chain cloned to two override layers.
               (-> world g/graph :nodes count))))))
  (testing "Connecting to override only spawns override nodes in subsequent override layers"
    (ts/with-clean-system
      ;; Test with one direct override of main, and one override of that override.
      (let [[main] (ts/tx-nodes (g/make-node world MainNode))
            [or-main] (ts/tx-nodes (g/override main))
            [or2-main] (ts/tx-nodes (g/override or-main))
            [new1-sub new2-sub new3-sub] (ts/tx-nodes (g/make-nodes world [new1-sub SubNode
                                                                           new2-sub SubNode
                                                                           new3-sub SubNode
                                                                           new3-sub-sub SubNode]
                                                        (g/connect new3-sub-sub :_node-id new3-sub :sub-nodes)))
            node-count-before-connection (-> world g/graph :nodes count)]
        (g/connect! new1-sub :_node-id or2-main :sub-nodes)
        (is (= node-count-before-connection)
            (-> world g/graph :nodes count)) ; Connecting to override with no subsequent overrides spawns no nodes.
        (g/connect! new2-sub :_node-id or-main :sub-nodes)
        (is (= (+ node-count-before-connection 1) ; Single node cloned to one override layer.
               (-> world g/graph :nodes count)))
        (g/connect! new3-sub :_node-id or-main :sub-nodes)
        (is (= (+ node-count-before-connection 1 2) ; Two-node chain cloned to one override layer.
               (-> world g/graph :nodes count)))))))

(deftest delete
  (ts/with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overrides can be removed"
        (g/transact (g/delete-node or-main))
        (doseq [nid [or-main or-sub]]
          (is (nil? (g/node-by-id nid))))
        (is (empty? (g/overrides main)))))
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overrides are removed with base"
        (g/transact (g/delete-node main))
        (doseq [nid [main sub or-main or-sub]]
          (is (nil? (g/node-by-id nid))))
        (is (empty? (g/overrides main)))))
    (let [[[main sub] [or-main or-sub] [or2-main or2-sub]] (setup world 2)]
      (testing "Delete hierarchy"
        (g/transact (g/delete-node main))
        (doseq [nid [main sub or-main or-sub or2-main or2-sub]]
          (is (nil? (g/node-by-id nid))))
        (is (empty? (g/overrides main)))))
    (let [[[main sub] [or-main or-sub]] (setup world 1)
          [stray] (ts/tx-nodes (g/make-nodes world
                                   [stray BaseNode]
                                 (g/connect stray :_node-id or-main :sub-nodes)))]
      (testing "Delete stray node attached to override :cascade-delete"
        (g/transact (g/delete-node main))
        (doseq [nid [main sub or-main or-sub stray]]
          (is (nil? (g/node-by-id nid))))
        (is (empty? (g/overrides main)))))))

(deftest override-property
  (ts/with-clean-system
    (let [[[main sub] [or-main or-sub]] (setup world 1)]
      (testing "Overriding property"
        (g/transact (g/set-property or-main :a-property "overridden main"))
        (is (= "overridden main" (g/node-value or-main :a-property)))
        (is (= "main" (g/node-value main :a-property)))
        (doseq [label [:_properties :_declared-properties]
                :let [props (-> (g/node-value or-main label) (get :properties))
                      a-p (:a-property props)
                      b-p (:b-property props)]]
          (is (= "overridden main" (:value a-p)))
          (is (= "main" (:original-value a-p)))
          (is (= or-main (:node-id a-p)))
          (is (= "main" (:value b-p)))
          (is (= false (contains? b-p :original-value)))
          (is (= or-main (:node-id b-p)))))
      (testing "Virtual property"
        (is (= "overridden main" (g/node-value or-main :virt-property))))
      (testing "Output production"
        (is (= "overridden main" (g/node-value or-main :cached-output)))
        (is (= "main" (g/node-value main :cached-output))))
      (testing "Clearing property"
        (g/transact (g/clear-property or-main :a-property))
        (is (= "main" (g/node-value or-main :a-property)))
        (is (= "main" (g/node-value or-main :virt-property)))
        (is (= "main" (g/node-value or-main :cached-output))))
      (testing "Update property"
        (g/transact (g/update-property or-main :a-property (fn [prop] (str prop "_changed"))))
        (is (= "main_changed" (g/node-value or-main :a-property)))))))

(deftest inherited-property
  (ts/with-clean-system
    (let [prop :base-property
          [main or-main] (ts/tx-nodes (g/make-nodes world [main [MainNode prop "inherited"]]
                                        (g/override main {})))]
      (is (= "inherited" (g/node-value or-main prop))))))

(deftest new-node-created
  (ts/with-clean-system
    (let [[main or-main sub] (ts/tx-nodes (g/make-nodes world [main MainNode]
                                            (g/override main {})
                                            (g/make-nodes world [sub SubNode]
                                              (g/connect sub :_node-id main :sub-nodes))))]
      (let [sub-nodes (g/node-value or-main :sub-nodes)]
        (is (= 1 (count sub-nodes)))
        (is (not= [sub] sub-nodes)))
      (g/transact (g/disconnect sub :_node-id main :sub-nodes))
      (is (empty? (g/node-value or-main :sub-nodes))))))

(deftest new-node-created-cache-invalidation
  (ts/with-clean-system
    (let [[main or-main] (ts/tx-nodes (g/make-nodes world [main MainNode]
                                        (g/override main {})))
          _ (is (= [] (g/node-value or-main :cached-values)))
          [sub] (ts/tx-nodes (g/make-nodes world [sub [SubNode :value "sub"]]
                               (for [[from to] [[:_node-id :sub-nodes]
                                                [:value :cached-values]]]
                                 (g/connect sub from main to))))]
      (is (= ["sub"] (g/node-value or-main :cached-values)))
      (g/transact (concat
                    (g/disconnect sub :_node-id main :sub-nodes)
                    (g/disconnect sub :value main :cached-values)))
      (is (= [] (g/node-value or-main :cached-values))))))

(g/defnode DetectCacheInvalidation
  (property invalid-cache g/Any)
  (input value g/Str :cascade-delete)
  (output cached-value g/Str :cached (g/fnk [value invalid-cache]
                                       (swap! invalid-cache inc)
                                       value)))

(deftest inherit-targets []
  (testing "missing override"
    (ts/with-clean-system
      (let [[main cache-node or-main] (ts/tx-nodes (g/make-nodes world [main [SubNode :value "test"]
                                                                        cache [DetectCacheInvalidation :invalid-cache (atom 0)]]
                                                     (g/connect main :value cache :value)
                                                     (g/override main {})))]
        (is (= cache-node (ffirst (g/targets-of main :value))))
        (is (empty? (g/targets-of or-main :value))))))
  (testing "existing override"
    (ts/with-clean-system
      (let [[main cache-node] (ts/tx-nodes (g/make-nodes world [main [SubNode :value "test"]
                                                                cache [DetectCacheInvalidation :invalid-cache (atom 0)]]
                                             (g/connect main :value cache :value)))
            [or-cache-node or-main] (ts/tx-nodes (g/override cache-node {}))]
        (is (= cache-node (ffirst (g/targets-of main :value))))
        (is (= or-cache-node (ffirst (g/targets-of or-main :value))))))))

(g/defnode StringInput
  (input value g/Str :cascade-delete))

(deftest inherit-sources []
  (testing "missing override"
    ;; After making g/override perform the bulk of its work in the
    ;; transaction step (after the g/connect has happened/is
    ;; observable) this test is less relevant.
    (ts/with-clean-system
      (let [[main cache-node or-main] (ts/tx-nodes (g/make-nodes world [main StringInput
                                                                        cache [DetectCacheInvalidation :invalid-cache (atom 0)]]
                                                     (g/connect cache :cached-value main :value)
                                                     (g/override main {:traverse-fn g/never-override-traverse-fn})))]
        (is (= cache-node (ffirst (g/sources-of main :value))))
        (is (= cache-node (ffirst (g/sources-of or-main :value)))))))
  (testing "existing override"
    (ts/with-clean-system
      (let [[main cache-node] (ts/tx-nodes (g/make-nodes world [main StringInput
                                                                cache [DetectCacheInvalidation :invalid-cache (atom 0)]]
                                             (g/connect cache :cached-value main :value)))
            [or-main or-cache-node] (ts/tx-nodes (g/override cache-node {}))]
        (is (= cache-node (ffirst (g/sources-of main :value))))
        (is (= or-cache-node (ffirst (g/sources-of or-main :value))))))))

(deftest lonely-override-leaves-cache []
  (ts/with-clean-system
    (let [[main cache-node or-main] (ts/tx-nodes (g/make-nodes world [main [SubNode :value "test"]
                                                                      cache [DetectCacheInvalidation :invalid-cache (atom 0)]]
                                                   (g/connect main :value cache :value)
                                                   (g/override main {})))]
      (is (= 0 @(g/node-value cache-node :invalid-cache)))
      (is (= "test" (g/node-value cache-node :cached-value)))
      (is (= 1 @(g/node-value cache-node :invalid-cache)))
      (is (= "test" (g/node-value cache-node :cached-value)))
      (is (= 1 @(g/node-value cache-node :invalid-cache)))
      (g/transact (g/set-property or-main :value "override"))
      (is (= 1 @(g/node-value cache-node :invalid-cache))))))

(deftest multi-override
  (ts/with-clean-system
    (let [[[main sub]
           [or-main or-sub]
           [or2-main or2-sub]] (setup world 2)]
      (testing "basic default behaviour"
        (is (= "main" (g/node-value or2-main :a-property))))
      (testing "cache-invalidation in multiple overrides"
        (g/transact (g/set-property main :a-property "new main"))
        (is (= "new main" (g/node-value or2-main :a-property))))
      (testing "override one below overrides"
        (g/transact (g/set-property or-main :a-property "main2"))
        (is (= "main2" (g/node-value or2-main :a-property))))
      (testing "top level override"
        (g/transact (g/set-property or2-main :a-property "main3"))
        (is (= "main3" (g/node-value or2-main :a-property)))))))

(defn- prop-map-value
  [node prop-kw]
  (-> node (g/node-value :_properties) :properties prop-kw :value))

(defn- prop-map-original-value
  [node prop-kw]
  (-> node (g/node-value :_properties) :properties prop-kw :original-value))

(deftest multi-override-with-dynamic-properties
  (testing "property value is propagated through all override nodes"
    (ts/with-clean-system
      (let [[[main sub]
             [or1-main or1-sub]
             [or2-main or2-sub]
             [or3-main or3-sub]] (setup world 3)]
        (is (every? #(= "main" (prop-map-value % :a-property)) [main or1-main or2-main or3-main]))
        (is (every? #(= "main" (prop-map-value % :c-property)) [main or1-main or2-main or3-main]))

        (g/set-property! or1-main :a-property "a")
        (is (= "main" (prop-map-value main :a-property)))
        (is (= "a" (prop-map-value or1-main :a-property)))
        (is (= "main" (prop-map-original-value or1-main :a-property)))
        (is (= "a" (prop-map-value or2-main :a-property)))
        (is (= nil (prop-map-original-value or2-main :a-property)))
        (is (= "a" (prop-map-value or3-main :a-property)))
        (is (= nil (prop-map-original-value or3-main :a-property)))

        (g/set-property! or2-main :a-property "b")
        (is (= "main" (prop-map-value main :a-property)))
        (is (= "a" (prop-map-value or1-main :a-property)))
        (is (= "main" (prop-map-original-value or1-main :a-property)))
        (is (= "b" (prop-map-value or2-main :a-property)))
        (is (= "a" (prop-map-original-value or2-main :a-property)))
        (is (= "b" (prop-map-value or3-main :a-property)))
        (is (= nil (prop-map-original-value or3-main :a-property)))

        (g/clear-property! or1-main :a-property)
        (g/clear-property! or2-main :a-property)
        (g/set-property! or1-main :c-property "c")
        (is (= "main" (prop-map-value main :c-property)))
        (is (= "c" (prop-map-value or1-main :c-property)))
        (is (= "main" (prop-map-original-value or1-main :c-property)))
        (is (= "c" (prop-map-value or2-main :c-property)))
        (is (= nil (prop-map-original-value or2-main :c-property)))
        (is (= "c" (prop-map-value or3-main :c-property)))
        (is (= nil (prop-map-original-value or3-main :c-property)))

        (g/set-property! or2-main :c-property "d")
        (is (= "main" (prop-map-value main :c-property)))
        (is (= "c" (prop-map-value or1-main :c-property)))
        (is (= "main" (prop-map-original-value or1-main :c-property)))
        (is (= "d" (prop-map-value or2-main :c-property)))
        (is (= "c" (prop-map-original-value or2-main :c-property)))
        (is (= "d" (prop-map-value or3-main :c-property)))
        (is (= nil (prop-map-original-value or3-main :c-property)))))))

(deftest mark-defective
  (ts/with-clean-system
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (testing "defective base"
        (let [error (g/error-fatal "Something went wrong" {:type :some-error})]
          (g/transact
            (g/mark-defective main error))
          (is (g/error? (g/node-value or-main :a-property))))))
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (testing "defective override, which throws exception"
        (let [error (g/error-fatal "Something went wrong" {:type :some-error})]
          (is (thrown? Exception (g/transact
                                   (g/mark-defective or-main error)))))))))

(deftest copy-paste
  (ts/with-clean-system
    (let [[[main sub]
           [or-main or-sub]] (setup world 1)]
      (g/transact (g/set-property or-main :a-property "override"))
      (let [fragment (g/copy [or-main] {:traverse? fn/constantly-true})
            paste-data (g/paste (g/node-id->graph-id or-main) fragment {})
            copy-id (first (:root-node-ids paste-data))]
        (g/transact (:tx-data paste-data))
        (is (= "override" (g/node-value copy-id :a-property)))))))

(g/defnode ExternalNode
  (property value g/Keyword))

(g/defnode ResourceNode
  (property reference g/Keyword
            (value (g/fnk [in-reference] in-reference))
            (set (fn [evaluation-context node-id old-value new-value]
                   (concat
                     (g/disconnect-sources (:basis evaluation-context) node-id :in-reference)
                     (let [gid (g/node-id->graph-id node-id)
                           node-store (g/graph-value gid :node-store)
                           src-id (get node-store new-value)]
                       (if src-id
                         (g/connect src-id :value node-id :in-reference)
                         []))))))
  (input in-reference g/Keyword))

(deftest connection-property
  (ts/with-clean-system
    (let [[res a b] (ts/tx-nodes (g/make-nodes world [res ResourceNode
                                                      a [ExternalNode :value :node-a]
                                                      b [ExternalNode :value :node-b]]
                                   (g/set-graph-value world :node-store {:node-a a :node-b b})))]
      (testing "linking through property setter"
        (g/transact (g/set-property res :reference :node-a))
        (is (= :node-a (g/node-value res :reference)))
        (g/transact (g/set-property a :value :node-a2))
        (is (= :node-a2 (g/node-value res :reference))))
      (let [or-res (first (override res))]
        (testing "override inherits the connection"
          (is (= :node-a2 (g/node-value or-res :reference))))
        (testing "override the actual connection"
          (g/transact (g/set-property or-res :reference :node-b))
          (is (= :node-b (g/node-value or-res :reference))))
        (testing "clearing the property"
          (g/transact (g/clear-property or-res :reference))
          (is (= :node-a2 (g/node-value or-res :reference))))))))

(g/deftype ^:private IDMap {s/Str s/Int})

(defprotocol Resource
  (path [this]))

(defrecord PathResource [path]
  Resource
  (path [this] path))

(defn properties->overrides [id properties]
  {id (->> (:properties properties)
           (filter (fn [[k v]] (contains? v :original-value)))
           (map (fn [[k v]] [k (:value v)]))
           (into {}))})

(g/defnode Node
  (property id g/Str)
  (input id-prefix g/Str)
  (output id g/Str (g/fnk [id-prefix id] (str id-prefix id)))
  (output node-ids IDMap (g/fnk [_node-id id] {id _node-id}))
  (output node-overrides g/Any (g/fnk [id _properties] (properties->overrides id _properties))))

(g/defnode VisualNode
  (inherits Node)
  (property value g/Str))


(g/defnode SceneResourceNode
  (property resource Resource :unjammable))

(g/defnode NodeTree
  (input nodes g/NodeID :array :cascade-delete)
  (input node-ids IDMap :array)
  (input node-overrides g/Any :array)
  (input id-prefix g/Str)
  (output id-prefix g/Str (g/fnk [id-prefix] id-prefix))
  (output node-ids IDMap :cached (g/fnk [node-ids] (reduce into {} node-ids)))
  (output node-overrides g/Any :cached (g/fnk [node-overrides]
                                         (reduce into {} node-overrides))))

(g/defnode Scene
  (inherits SceneResourceNode)
  (input node-tree g/NodeID :cascade-delete)
  (input layouts g/NodeID :array :cascade-delete)
  (input id-prefix g/Str)
  (output id-prefix g/Str (g/fnk [id-prefix] id-prefix))
  (input node-ids IDMap)
  (output node-ids IDMap (g/fnk [node-ids] node-ids))
  (input node-overrides g/Any)
  (output node-overrides g/Any (g/fnk [node-overrides] node-overrides)))

(defn- scene-by-path
  ([graph path]
   (scene-by-path (g/now) graph path))
  ([basis graph path]
   (let [resources (or (g/graph-value basis graph :resources) {})]
     (get resources (->PathResource path)))))

(g/defnode Template
  (inherits Node)
  (property template g/Any
            (value (g/fnk [template-resource source-overrides]
                     {:resource template-resource :overrides source-overrides}))
            (set (fn [evaluation-context self old-value new-value]
                   (let [basis (:basis evaluation-context)
                         current-scene (g/node-feeding-into basis self :template-resource)]
                     (concat
                       (if current-scene
                         (g/delete-node current-scene)
                         [])
                       (let [gid (g/node-id->graph-id self)
                             path (:path new-value)]
                         (if-let [scene (scene-by-path basis gid path)]
                           (let [tmpl-path (g/node-value self :template-path evaluation-context)
                                 properties-by-node-id (comp (or (:overrides new-value) {})
                                                             (into {} (map (fn [[k v]] [v (str tmpl-path k)]))
                                                                   (g/node-value scene :node-ids evaluation-context)))]
                             (g/override scene {:properties-by-node-id properties-by-node-id}
                                         (fn [evaluation-context id-mapping]
                                           (let [or-scene (id-mapping scene)]
                                             (concat
                                               (for [[from to] [[:node-ids :node-ids]
                                                                [:node-overrides :source-overrides]
                                                                [:resource :template-resource]]]
                                                 (g/connect or-scene from self to))
                                               (g/connect self :template-path or-scene :id-prefix))))))
                           [])))))))
  (input template-resource Resource :cascade-delete)
  (input node-ids IDMap)
  (input instance g/NodeID)
  (input source-overrides g/Any)
  (output template-path g/Str (g/fnk [id] (str id "/")))
  (output node-overrides g/Any :cached (g/fnk [id _properties source-overrides]
                                         (merge (properties->overrides id _properties) source-overrides)))
  (output node-ids IDMap (g/fnk [_node-id id node-ids] (into {id _node-id} node-ids))))

(g/defnode Layout
  (property name g/Str)
  (property nodes g/Any
            (set (fn [evaluation-context self _ new-value]
                   (let [basis (:basis evaluation-context)
                         current-tree (g/node-feeding-into basis self :node-tree)]
                     (concat
                       (if current-tree
                         (g/delete-node current-tree)
                         [])
                       (let [scene (ffirst (g/targets-of basis self :_node-id))
                             node-tree (g/node-value scene :node-tree evaluation-context)]
                         (g/override node-tree {}
                                     (fn [evaluation-context id-mapping]
                                       (let [node-tree-or (id-mapping node-tree)]
                                         (for [[from to] [[:_node-id :node-tree]]]
                                           (g/connect node-tree-or from self to)))))))))))
  (input node-tree g/NodeID :cascade-delete)
  (input id-prefix g/Str))

(def ^:private node-tree-inputs [[:_node-id :nodes]
                                 [:node-ids :node-ids]
                                 [:node-overrides :node-overrides]])

(def ^:private node-tree-outputs [[:id-prefix :id-prefix]])

(defn- add-node [graph scene node-tree node-type props]
  (g/make-nodes graph [n [node-type props]]
    (for [[output input] node-tree-inputs]
      (g/connect n output node-tree input))
    (for [[output input] node-tree-outputs]
      (g/connect node-tree output n input))))

(defn- add-layout [graph scene name]
  (g/make-nodes graph [n [Layout :name name]]
    (g/connect n :_node-id scene :layouts)
    (g/connect scene :id-prefix n :id-prefix)
    (g/set-property n :nodes {})))

(defn- load-scene [graph path nodes]
  (let [resource (->PathResource path)]
    (g/make-nodes graph [scene [Scene :resource resource]
                         node-tree [NodeTree]]
      (for [[from to] [[:_node-id :node-tree]
                       [:node-ids :node-ids]
                       [:node-overrides :node-overrides]]]
        (g/connect node-tree from scene to))
      (for [[from to] [[:id-prefix :id-prefix]]]
        (g/connect scene from node-tree to))
      (g/update-graph-value graph :resources assoc resource scene)
      (reduce (fn [tx [node-type props]]
                (into tx (add-node graph scene node-tree node-type props)))
              [] nodes))))

(defn- make-scene! [graph path nodes]
  (when (nil? (g/graph-value graph :resources))
    (g/set-graph-value! graph :resources {}))
  (ts/tx-nodes (load-scene graph path nodes)))

(defn- has-node? [scene node-id]
  (contains? (g/node-value scene :node-ids) node-id))

(defn- node-by-id [scene node-id]
  (get (g/node-value scene :node-ids) node-id))

(defn- target [n label]
  (ffirst (g/targets-of n label)))

(deftest scene-loading
  (ts/with-clean-system
    (let [[scene _ node] (make-scene! world "my-scene" [[VisualNode {:id "my-node" :value "initial"}]])
          overrides {"my-template/my-node" {:value "new value"}}
          [super-scene _ template] (make-scene! world "my-super-scene" [[Template {:id "my-template" :template {:path "my-scene" :overrides overrides}}]])]
      (is (= "initial" (g/node-value node :value)))
      (let [or-node (get (g/node-value template :node-ids) "my-template/my-node")]
        (is (= "new value" (g/node-value or-node :value))))
      (is (= overrides (select-keys (g/node-value template :node-overrides) (keys overrides)))))))

(deftest scene-loading-with-override-values
  (ts/with-clean-system
    (let [scene-overrides {"template/my-node" {:value "scene-override"}}
          super-overrides {"super-template/template/my-node" {:value "super-override"}}]
      (g/transact (concat
                    (load-scene world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
                    (load-scene world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides scene-overrides}}]])
                    (load-scene world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides super-overrides}}]])))
      (doseq [[actual expected] (mapv (fn [[s-path id expected]] [(-> s-path
                                                                      (->> (scene-by-path world))
                                                                      (node-by-id id)
                                                                      (g/node-value :node-overrides))
                                                                  expected])
                                      [["scene" "template" scene-overrides]
                                       ["super-scene" "super-template" super-overrides]])]
        (is (= expected (select-keys actual (keys expected))))))))

(deftest hierarchical-ids
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])]
      (is (has-node? sub-scene "my-node"))
      (is (has-node? scene "template/my-node"))
      (is (has-node? super-scene "super-template/template/my-node"))
      (let [tmp (node-by-id super-scene "super-template/template")]
        (g/transact (g/set-property sub-scene :resource (->PathResource "sub-scene2")))
        (is (= "sub-scene2" (get-in (g/node-value tmp :template) [:resource :path])))))))

(deftest delete-middle
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])
          middle (node-by-id scene "template")
          super-middle (node-by-id super-scene "super-template/template")]
      (is (has-node? super-scene "super-template/template"))
      (g/transact (g/delete-node middle))
      (is (not (has-node? super-scene "super-template/template")))
      (is (nil? (g/node-by-id super-middle))))))

(deftest sibling-templates
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]
                                              [Template {:id "template1" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])]
      (is (has-node? scene "template/my-node"))
      (is (has-node? scene "template1/my-node"))
      (is (has-node? super-scene "super-template/template/my-node"))
      (is (has-node? super-scene "super-template/template1/my-node")))))

(deftest new-sibling-delete-repeat
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene scene-tree] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])]
      (dotimes [i 5]
        (let [[new-tmpl] (ts/tx-nodes (add-node world scene scene-tree Template {:id "new-template" :template {:path "sub-scene" :overrides {}}}))]
          (is (contains? (g/node-value (node-by-id scene "new-template") :node-overrides) "new-template/my-node"))
          (g/transact (g/delete-node new-tmpl)))))))

(deftest change-override
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])
          [sub-scene2] (make-scene! world "sub-scene2" [[VisualNode {:id "my-node2" :value ""}]])]
      (g/transact (g/transfer-overrides {sub-scene sub-scene2}))
      (is (empty? (g/overrides sub-scene)))
      (is (has-node? super-scene "super-template/template/my-node2")))))

(deftest new-node-deep-override
  (ts/with-clean-system
    (let [[sub-scene sub-tree] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])
          [super-super-scene] (make-scene! world "super-super-scene" [[Template {:id "super-super-template" :template {:path "super-scene" :overrides {}}}]])]
      (g/transact (add-node world sub-scene sub-tree VisualNode {:id "my-node2"}))
      (is (has-node? super-super-scene "super-super-template/super-template/template/my-node2")))))

;; Bug occurring in properties in overloads

(deftest scene-paths
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])
          template (node-by-id super-scene "super-template/template")
          or-scene (ffirst (g/sources-of template :template-resource))]
      (is (= "sub-scene" (:path (g/node-value (g/override-original or-scene) :resource)))))))

;; Simulated layout problem

(deftest template-layouts
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]])
          [sub-layout] (ts/tx-nodes (add-layout world sub-scene "Portrait"))]
      (is (nil? (g/node-value sub-layout :id-prefix)))
      (is (not (nil? (g/node-value (first (g/overrides sub-layout)) :id-prefix)))))))

;; User-defined dynamic properties

(g/defnode Script
  (property script-properties g/Any)
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties script-properties]
                                             (-> _declared-properties
                                                 (update :properties dissoc :script-properties)
                                                 (update :properties merge (into {} (map (fn [[key value]] [key {:value value
                                                                                                                 :type (type value)
                                                                                                                 :node-id _node-id}]) script-properties)))
                                                 (update :display-order (comp vec (partial remove #{:script-properties})))))))

(g/defnode Component
  (property id g/Str)
  (property component g/Any
            (set (fn [evaluation-context self old-value new-value]
                   (let [basis (:basis evaluation-context)]
                     (concat
                       (if-let [instance (g/node-value self :instance evaluation-context)]
                         (g/delete-node instance)
                         [])
                       (let [gid (g/node-id->graph-id self)
                             path (:path new-value)]
                         (if-let [script (get (g/graph-value basis gid :resources) path)]
                           (g/override script {}
                                       (fn [evaluation-context id-mapping]
                                         (let [or-script (id-mapping script)
                                               script-props (g/node-value script :_properties evaluation-context)
                                               set-prop-data (for [[key value] (:overrides new-value)]
                                                               (g/set-property or-script key value))
                                               conn-data (for [[src tgt] [[:_node-id :instance]
                                                                          [:_properties :script-properties]]]
                                                           (g/connect or-script src self tgt))]
                                           (concat set-prop-data conn-data))))
                           [])))))))
  (input instance g/NodeID :cascade-delete)
  (input script-properties g/Properties)
  (output _properties g/Properties (g/fnk [_declared-properties script-properties]
                                     (-> _declared-properties
                                         (update :properties merge (:properties script-properties))
                                         (update :display-order concat (:display-order script-properties))))))

(deftest custom-properties
  (ts/with-clean-system
    (let [[script comp] (ts/tx-nodes (g/make-nodes world [script [Script :script-properties {:speed 0}]]
                                       (g/set-graph-value world :resources {"script.script" script}))
                                     (g/make-nodes world [comp [Component :component {:path "script.script" :overrides {:speed 10}}]]))]
      (let [p (get-in (g/node-value comp :_properties) [:properties :speed])]
        (is (= 10 (:value p)))
        (is (= 0 (:original-value p)))
        (g/transact (g/set-property (:node-id p) :speed 20))
        (let [p' (get-in (g/node-value comp :_properties) [:properties :speed])]
          (is (= 20 (:value p')))
          (is (= 0 (:original-value p'))))
        (g/transact (g/clear-property (:node-id p) :speed))
        (let [p' (get-in (g/node-value comp :_properties) [:properties :speed])]
          (is (= 0 (:value p')))
          (is (not (contains? p' :original-value))))))))

;; Overloaded outputs with different types

(g/deftype XYZ [(s/one s/Num "x") (s/one s/Num "y") (s/one s/Num "z")])

(g/deftype Complex {s/Keyword Vector3d})

(g/defnode TypedOutputNode
  (property value XYZ)
  (output value Vector3d (g/fnk [value] (let [[x y z] value] (Vector3d. x y z))))
  (output complex Complex (g/fnk [value] {:value value})))

(deftest overloaded-outputs-and-types
  (ts/with-clean-system
    (let [[a b] (ts/tx-nodes (g/make-nodes world [n [TypedOutputNode :value [1 2 3]]]
                               (g/override n)))]
      (g/transact (g/set-property b :value [2 3 4]))
      (is (not= (g/node-value b :complex) (g/node-value a :complex))))))

;; Dynamic property production

(g/defnode IDNode
  (input super-id g/Str)
  (property id g/Str (value (g/fnk [_node-id super-id id] (if super-id (str super-id "/" id) id)))))

(deftest dynamic-id-in-properties
  (ts/with-clean-system
    (let [[node parent sub] (ts/tx-nodes (g/make-nodes world [node [IDNode :id "child-id"]
                                                              parent [IDNode :id "parent-id"]]
                                           (g/override node {}
                                                       (fn [evaluation-context id-mapping]
                                                         (let [or-node (id-mapping node)]
                                                           (g/connect parent :id or-node :super-id))))))]
      (is (= (g/node-value sub :id)
             (get-in (g/node-value sub :_declared-properties) [:properties :id :value]))))))

;; Reload supported for overrides

(deftest reload-overrides
  (ts/with-clean-system
    (let [[node or-node] (ts/tx-nodes (g/make-nodes world [node [support/ReloadNode :my-value "reload-test"]]
                                        (g/override node)))]
      (g/transact (g/set-property or-node :my-value "new-value"))
      (is (= "new-value" (get-in (g/node-value or-node :_properties) [:properties :my-value :value])))
      (use 'dynamo.integration.override-test-support :reload)
      (is (= "new-value" (get-in (g/node-value or-node :_properties) [:properties :my-value :value]))))))

;; Dependency rules

(defn- outs [nodes output]
  (for [n nodes]
    (gt/endpoint n output)))

(defn- conn? [[src src-label tgt tgt-label]]
  (let [basis (g/now)]
    (and (g/connected? basis src src-label tgt tgt-label)
         (contains? (into #{} (gt/sources basis tgt tgt-label)) [src src-label])
         (contains? (into #{} (gt/targets basis src src-label)) [tgt tgt-label]))))

(defn- no-conn? [[src src-label tgt tgt-label]]
  (let [basis (g/now)]
    (and (not (g/connected? basis src src-label tgt tgt-label))
         (not (contains? (into #{} (gt/sources basis tgt tgt-label)) [src src-label]))
         (not (contains? (into #{} (gt/targets basis src src-label)) [tgt tgt-label])))))

(defn- deps [tgts]
  (ts/graph-dependencies tgts))

(g/defnode TargetNode
  (input in-value g/Str)
  (output out-value g/Str (g/fnk [in-value] in-value)))

(deftest dep-rules
  (ts/with-clean-system
    (let [all (setup world 2)
          [[main-0 sub-0]
           [main-1 sub-1]
           [main-2 sub-2]] all
          mains (mapv first all)
          subs (mapv second all)]
      (testing "value fnk"
        (is (every? (set (deps [(gt/endpoint main-0 :a-property)])) (outs mains :virt-property))))
      (testing "output"
        (is (every? (set (deps [(gt/endpoint main-0 :a-property)])) (outs mains :cached-output))))
      (testing "connections"
        (is (every? conn? (for [[m s] all]
                            [s :_node-id m :sub-nodes])))
        (is (every? no-conn? (for [mi (range 3)
                                   si (range 3)
                                   :when (not= mi si)
                                   :let [m (nth mains mi)
                                         s (nth subs si)]]
                               [s :_node-id m :sub-nodes]))))))
  (ts/with-clean-system
    (let [[src tgt src-1] (ts/tx-nodes (g/make-nodes world [src [MainNode :a-property "reload-test"]
                                                            tgt TargetNode]
                                         (g/connect src :a-property tgt :in-value)
                                         (g/override src)))]
      (testing "regular dep"
        (is (every? (set (deps [(gt/endpoint src :a-property)])) [(gt/endpoint tgt :out-value)])))
      (testing "no override deps"
        (is (not-any? (set (deps [(gt/endpoint src-1 :a-property)])) [(gt/endpoint tgt :out-value)])))
      (testing "connections"
        (is (conn? [src :a-property tgt :in-value]))
        (is (no-conn? [src-1 :a-property tgt :in-value])))))
  (ts/with-clean-system
    (let [[src tgt tgt-1] (ts/tx-nodes (g/make-nodes world [src [MainNode :a-property "reload-test"]
                                                            tgt TargetNode]
                                         (g/connect src :a-property tgt :in-value)
                                         (g/override tgt)))]
      (testing "regular dep"
        (is (every? (set (deps [(gt/endpoint src :a-property)]))
                    [(gt/endpoint tgt :out-value)
                     (gt/endpoint tgt-1 :out-value)])))
      (testing "connections"
        (is (conn? [src :a-property tgt :in-value]))
        (is (conn? [src :a-property tgt-1 :in-value])))))
  (ts/with-clean-system
    (let [[sub-scene] (make-scene! world "sub-scene" [[VisualNode {:id "my-node" :value ""}]])
          [scene] (make-scene! world "scene" [[Template {:id "template" :template {:path "sub-scene" :overrides {}}}]
                                              [Template {:id "template1" :template {:path "sub-scene" :overrides {}}}]])
          [super-scene] (make-scene! world "super-scene" [[Template {:id "super-template" :template {:path "scene" :overrides {}}}]])
          [tmpl-scene tmpl1-scene] (g/overrides sub-scene)
          [tmpl-tree tmpl1-tree] (mapv #(g/node-value % :node-tree) [tmpl-scene tmpl1-scene])
          [tmpl-sub tmpl1-sub] (mapv #(node-by-id scene %) ["template/my-node" "template1/my-node"])]
      (is (conn? [tmpl-sub :node-overrides tmpl-tree :node-overrides]))
      (is (conn? [tmpl1-sub :node-overrides tmpl1-tree :node-overrides]))
      (is (conn? [tmpl-tree :node-overrides tmpl-scene :node-overrides]))
      (is (conn? [tmpl1-tree :node-overrides tmpl1-scene :node-overrides])))))

(g/defnode GameObject
  (input components g/NodeID :array :cascade-delete))

(g/defnode GameObjectInstance
  (input source g/NodeID :cascade-delete))

(g/defnode Collection
  (input instances g/NodeID :array :cascade-delete))

(deftest override-created-on-connect
  (ts/with-clean-system
    (let [[script] (ts/tx-nodes (g/make-nodes world [script Script]))
          [go comp or-script] (ts/tx-nodes (g/make-nodes world [go GameObject
                                                                comp Component]
                                             (g/override script {:traverse-fn g/always-override-traverse-fn}
                                                         (fn [evaluation-context id-mapping]
                                                           (let [or-script (id-mapping script)]
                                                             (concat
                                                               (g/connect comp :_node-id go :components)
                                                               (g/connect or-script :_node-id comp :instance)))))))
          [coll inst or-go] (ts/tx-nodes (g/make-nodes world [coll Collection
                                                              inst GameObjectInstance]
                                           (g/override go {:traverse-fn g/always-override-traverse-fn}
                                                       (fn [evaluation-context id-mapping]
                                                         (let [or-go (id-mapping go)]
                                                           (concat
                                                             (g/connect inst :_node-id coll :instances)
                                                             (g/connect or-go :_node-id inst :source)))))))
          [comp-2 or-script-2] (ts/tx-nodes (g/make-nodes world [comp Component]
                                              (g/override script {:traverse-fn g/always-override-traverse-fn}
                                                          (fn [evaluation-context id-mapping]
                                                            (let [or-script (id-mapping script)]
                                                              (g/connect or-script :_node-id comp :instance))))))
          nodes-on-connect (ts/tx-nodes (g/connect comp-2 :_node-id go :components))]
      ;; 1 override for the component node and one for the script, w.r.t the collection
      (is (= 2 (count nodes-on-connect))))))

(deftest cascade-delete-avoided
  (ts/with-clean-system
    (let [[script] (ts/tx-nodes (g/make-nodes world [script Script]))
          [go comp or-script] (ts/tx-nodes (g/make-nodes world [go GameObject
                                                                comp Component]
                                             (g/override script {:traverse-fn g/always-override-traverse-fn}
                                                         (fn [evaluation-context id-mapping]
                                                           (let [or-script (id-mapping script)]
                                                             (concat
                                                               (g/connect comp :_node-id go :components)
                                                               (g/connect or-script :_node-id comp :instance)))))))
          [coll inst or-go] (ts/tx-nodes (g/make-nodes world [coll Collection
                                                              inst GameObjectInstance]
                                           (g/override go {:traverse-fn g/never-override-traverse-fn}
                                                       (fn [evaluation-context id-mapping]
                                                         (let [or-go (id-mapping go)]
                                                           (concat
                                                             (g/connect inst :_node-id coll :instances)
                                                             (g/connect or-go :_node-id inst :source)))))))]
      (is (every? some? (map g/node-by-id [coll inst or-go go comp or-script script])))
      (g/transact (g/delete-node coll))
      (is (every? nil? (map g/node-by-id [coll inst or-go])))
      (is (every? some? (map g/node-by-id [go comp or-script script])))
      (g/transact (g/delete-node go))
      (is (every? nil? (map g/node-by-id [coll inst or-go go comp or-script])))
      (is (every? some? (map g/node-by-id [script])))
      (g/transact (g/delete-node script))
      (is (every? nil? (map g/node-by-id [coll inst or-go go comp or-script script]))))))

(deftest auto-add-and-delete
  (ts/with-clean-system
    (let [[script] (ts/tx-nodes (g/make-nodes world [script Script]))
          [go] (ts/tx-nodes (g/make-nodes world [go GameObject]))
          [[coll0 go-inst0 or-go0]
           [coll1 go-inst1 or-go1]] (for [i (range 2)]
                                      (ts/tx-nodes (g/make-nodes world [coll Collection
                                                                        go-inst GameObjectInstance]
                                                     (g/connect go-inst :_node-id coll :instances)
                                                     (g/override go {:traverse-fn g/always-override-traverse-fn}
                                                                 (fn [evaluation-context id-mapping]
                                                                   (let [or-go (id-mapping go)]
                                                                     (g/connect or-go :_node-id go-inst :source)))))))
          [comp or-script] (ts/tx-nodes (g/make-nodes world [comp Component]
                                          (g/override script {:traverse-fn g/always-override-traverse-fn}
                                                      (fn [evaluation-context id-mapping]
                                                        (let [or-script (id-mapping script)]
                                                          (concat
                                                            (g/connect or-script :_node-id comp :instance)
                                                            (g/connect comp :_node-id go :components)))))))]
      (let [all-script-nodes (doall (tree-seq fn/constantly-true g/overrides script))]
        (is (= 4 (count all-script-nodes)))
        (g/transact (g/delete-node comp))
        (is (= 1 (count (keep g/node-by-id all-script-nodes))))
        (is (empty? (mapcat g/overrides all-script-nodes)))))))

(defn- remove-idx [v ix]
  (into (subvec v 0 ix) (subvec v (inc ix))))

(defn- all-system-nodes []
  (into []
        (mapcat #(keys (:nodes (second %))))
        (:graphs @g/*the-system*)))

(deftest symmetric-input-output-arcs
  (test-util/with-loaded-project "test/resources/override_project"
    (let [all-nodes (all-system-nodes)
          node-outputs (reduce (fn [result n]
                                 (reduce (fn [result label]
                                           (assoc-in result [n label] (vec (g/labelled-outputs n label))))
                                         result
                                         (g/output-labels (g/node-type* n))))
                               {}
                               all-nodes)
          node-inputs (reduce (fn [result n]
                                (reduce (fn [result label]
                                          (assoc-in result [n label] (vec (g/labelled-inputs n label))))
                                        result
                                        (g/input-labels (g/node-type* n))))
                              {}
                              all-nodes)]
      (testing "outputs and labelled-outputs report the same arcs, and same cardinality of arcs"
        (doseq [node all-nodes]
          (let [outputs (g/outputs node)
                outputs-freqs (frequencies outputs)
                merged-outputs (into [] cat (vals (node-outputs node)))
                merged-outputs-freqs (frequencies merged-outputs)
                freq-diff [:all-merged (not-empty (set/difference (set outputs-freqs) (set merged-outputs-freqs)))
                           :merged-all (not-empty (set/difference (set merged-outputs-freqs) (set outputs-freqs)))]]
            (is (= freq-diff [:all-merged nil :merged-all nil])))))
      (testing "inputs and labelled-inputs report the same arcs, and same cardinality of arcs"
        (doseq [node all-nodes]
          (let [inputs (g/inputs node)
                inputs-freqs (frequencies inputs)
                merged-inputs (into [] cat (vals (node-inputs node)))
                merged-inputs-freqs (frequencies merged-inputs)
                freq-diff [:all-merged (not-empty (set/difference (set inputs-freqs) (set merged-inputs-freqs)))
                           :merged-all (not-empty (set/difference (set merged-inputs-freqs) (set inputs-freqs)))]]
            (is (= freq-diff [:all-merged nil :merged-all nil])))))
      (testing "For all outputs, there is a corresponding input and vice versa"
        (let [inputs (atom node-inputs)] ; we tick off one input arc per output arc
          (doseq [node all-nodes]
            (loop [outputs (g/outputs node)]
              (when (seq outputs)
                (let [output (first outputs)
                      target (nth output 2)
                      target-label (nth output 3)
                      input-index (first (util/positions #(= output %) (get-in @inputs [target target-label])))]
                  (is (some? input-index) (str "missing input for " output " in:\n" (get-in @inputs [target target-label])))
                  (swap! inputs update-in [target target-label] remove-idx input-index)
                  (recur (rest outputs))))))
          (is (empty? (into [] (comp (map vals) cat cat) (vals @inputs)))))) ; should end up with no input arcs left
      (testing "Sanity: all inputs == all outputs"
        (let [output-freqs (frequencies (mapcat g/outputs all-nodes))
              input-freqs (frequencies (mapcat g/inputs all-nodes))]
          (is (= output-freqs input-freqs)))))))
