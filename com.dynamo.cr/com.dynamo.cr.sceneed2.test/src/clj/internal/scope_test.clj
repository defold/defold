(ns internal.scope-test
  (:require [clojure.core.async :as a]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [clojure.test.check :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system.test-support :refer :all]
            [dynamo.system :as ds :refer [   ]]
            [dynamo.types :as t]
            [internal.async :as ia]
            [internal.node :as in])
  (:import [dynamo.types Image AABB]))

(n/defnode N1)
(n/defnode N2)

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

(n/defnode ParticleEditor
  (inherits n/Scope))

(n/defnode Emitter
  (property name s/Str))
(n/defnode Modifier
  (property name s/Str))

(defn solo [ss] (or (first ss) (throw (ex-info (str "Exactly one result was expected. Got " (count ss)) {}))))

(defn q [w clauses] (solo (ds/query w clauses)))

(deftest scope-registration
  (testing "Nodes are registered within a scope by name"
    (with-clean-world
      (ds/transactional
        (ds/in (ds/add (n/construct ParticleEditor :label "view scope"))
          (ds/add (n/construct Emitter  :name "emitter"))
          (ds/add (n/construct Modifier :name "vortex"))))

      (let [scope-node (q world-ref [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q world-ref [[:name n]]))
                 "emitter"
                 "vortex")))))

(n/defnode DisposableNode
  t/IDisposable
  (dispose [this] (deliver (:latch this) true)))

(def gen-disposable-node
  (gen/fmap (fn [_] (n/construct DisposableNode)) (gen/return 1)))

(def gen-nodelist
  (gen/vector gen-disposable-node))

(defspec scope-disposes-contained-nodes
  (prop/for-all [scoped-nodes gen-nodelist]
    (with-clean-world
      (let [scope          (ds/transactional (ds/add (n/construct n/Scope)))
            disposables    (ds/transactional (ds/in scope (doseq [n scoped-nodes] (ds/add n))) scoped-nodes)
            disposable-ids (map :_id disposables)]
        (ds/transactional (ds/delete scope))
        (let [last-tx   (:last-tx @world-ref)
              disposals (:values-to-dispose last-tx)]
          (is (= (sort disposable-ids) (sort (map :_id disposals)))))))))
