(ns internal.scope-test
  (:require [clojure.core.async :as a]
            [clojure.string :as str]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check :refer :all]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.async :as ia]
            [internal.node :as in]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [schema.macros :as sm])
  (:import [dynamo.types Image AABB]))

(g/defnode N1)
(g/defnode N2)

(deftest input-compatibility
  (let [n1 (n/construct N1)
        n2 (n/construct N2)]
    (are [out-node out out-type in-node in in-type expect-compat why]
      (= expect-compat (in/compatible? [out-node out out-type in-node in in-type]))
      n1 :image Image    n2 :image  AABB      nil                    "type mismatch"
      n1 :image Image    n2 :image  Image     [n1 :image n2 :image]  "ok"
      n1 :image Image    n2 :images [Image]   [n1 :image n2 :images] "ok"
      n1 :image Image    n2 :images Image     nil                    "plural name, singular type"
      n1 :name  String   n2 :names  [String]  [n1 :name n2 :names]   "ok"
      n1 :name  String   n2 :names  String    nil                    "plural name, singular type"
      n1 :names [String] n2 :names  [String]  [n1 :names n2 :names]  "ok"
      n1 :name  String   n2 :name   [String]  nil                    "singular name, plural type")))

(g/defnode ParticleEditor
  (inherits g/Scope))

(g/defnode Emitter
  (property name s/Str))
(g/defnode Modifier
  (property name s/Str))

(defn solo [ss] (or (first ss) (throw (ex-info (str "Exactly one result was expected. Got " (count ss)) {}))))

(defn q [w clauses] (solo (ds/query w clauses)))

(deftest scope-registration
  (testing "Nodes are registered within a scope by name"
    (with-clean-system
      (g/transactional
        (ds/in (g/add (n/construct ParticleEditor :label "view scope"))
          (g/add (n/construct Emitter  :name "emitter"))
          (g/add (n/construct Modifier :name "vortex"))))

      (let [scope-node (q world-ref [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q world-ref [[:name n]]))
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
      (let [scope          (g/transactional (g/add (n/construct g/Scope)))
            disposables    (g/transactional (ds/in scope (doseq [n scoped-nodes] (g/add n))) scoped-nodes)
            disposable-ids (map :_id disposables)]
        (g/transactional (g/delete scope))
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= (sort (conj disposable-ids (:_id scope))) (sort (map :_id disposed)))))))))
