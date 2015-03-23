(ns internal.scope-test
  (:require [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check :refer :all]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.node :as in]
            [schema.macros :as sm]
            [editor.core :as core]
            [internal.system :as is]))

;; TODO - move this to editor. It's no longer an inherent part of the graph.

(sm/defrecord T1 [ident :- String])
(sm/defrecord T2 [value :- Integer])

(deftest type-compatibility
  (are [first second allow-collection? compatible?]
    (= compatible? (n/type-compatible? first second allow-collection?))
    T1 T1               false    true
    T1 T1               true     false
    T1 T2               false    false
    T1 T2               true     false
    T1 [T1]             true     true
    T1 [T1]             false    false
    T1 [T2]             true     false
    T1 [T2]             false    false
    String String       false    true
    String String       true     false
    String [String]     false    false
    String [String]     true     true
    [String] String     false    false
    [String] String     true     false
    [String] [String]   false    true
    [String] [String]   true     false
    [String] [[String]] true     true
    Integer  Number     false    true
    Integer  t/Num      false    true
    T1       t/Any      false    true
    T1       t/Any      true     true
    T1       [t/Any]    false    false
    T1       [t/Any]    true     true
    String   t/Any      false    true
    String   t/Any      true     true
    String   [t/Any]    false    false
    String   [t/Any]    true     true
    [String] t/Any      false    true
    [String] t/Any      true     true
    [String] [t/Any]    false    true
    [String] [t/Any]    false    true))

(deftype ABACAB [])
(deftype Image [])

(g/defnode N1)
(g/defnode N2)

(deftest input-compatibility
  (let [n1 (n/construct N1)
        n2 (n/construct N2)]
    (are [out-node out out-type in-node in in-type expect-compat why]
      (= expect-compat (n/compatible? [out-node out out-type in-node in in-type]))
      n1 :image Image    n2 :image  ABACAB    nil                    "type mismatch"
      n1 :image Image    n2 :image  Image     [n1 :image n2 :image]  "ok"
      n1 :image Image    n2 :images [Image]   [n1 :image n2 :images] "ok"
      n1 :image Image    n2 :images Image     nil                    "plural name, singular type"
      n1 :name  String   n2 :names  [String]  [n1 :name n2 :names]   "ok"
      n1 :name  String   n2 :names  String    nil                    "plural name, singular type"
      n1 :names [String] n2 :names  [String]  [n1 :names n2 :names]  "ok"
      n1 :name  String   n2 :name   [String]  nil                    "singular name, plural type")))

(g/defnode ParticleEditor
  (inherits core/Scope))

(g/defnode Emitter
  (property name t/Str))
(g/defnode Modifier
  (property name t/Str))

(defn solo [ss] (or (first ss) (throw (ex-info (str "Exactly one result was expected. Got " (count ss)) {}))))

(defn q [g clauses] (solo (ds/query g clauses)))

(deftest scope-registration
  (testing "Nodes are registered within a scope by name"
    (with-clean-system
      (g/transactional
        (ds/in (g/add (n/construct ParticleEditor :label "view scope"))
          (g/add (n/construct Emitter  :name "emitter"))
          (g/add (n/construct Modifier :name "vortex"))))

      (let [scope-node (q (is/world-graph system) [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q (is/world-graph system) [[:name n]]))
                 "emitter"
                 "vortex")))))

(g/defnode DisposableNode
  t/IDisposable
  (dispose [this] (deliver (:latch this) true)))

(def gen-disposable-node
  (gen/fmap (fn [_] (n/construct DisposableNode)) (gen/return 1)))

(def gen-nodelist
  (gen/vector gen-disposable-node))

(defspec scope-disposes-contained-nodes
  (prop/for-all [scoped-nodes gen-nodelist]
    (with-clean-system
      (let [scope          (g/transactional (g/add (n/construct core/Scope)))
            disposables    (g/transactional (ds/in scope (doseq [n scoped-nodes] (g/add n))) scoped-nodes)
            disposable-ids (map :_id disposables)]
        (g/transactional (g/delete scope))
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= (sort (conj disposable-ids (:_id scope))) (sort (map :_id disposed)))))))))
