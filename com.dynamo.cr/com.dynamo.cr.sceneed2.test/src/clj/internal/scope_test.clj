(ns internal.scope-test
  (:require [clojure.core.async :as a]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [clojure.test.check :refer :all]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]]
            [dynamo.node :refer :all]
            [dynamo.project :refer [Project make-project]]
            [dynamo.system.test-support :refer :all]
            [dynamo.system :as ds :refer [transactional add in delete]]
            [dynamo.types :as t]
            [internal.async :as ia]
            [internal.query :as iq]
            [internal.node :as in])
  (:import [dynamo.types Image AABB]))

(defnode N1)
(defnode N2)

(deftest input-compatibility
  (let [n1 (make-n-1)
        n2 (make-n-2)]
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

(defnode ParticleEditor
  (inherits Scope))

(defnode Emitter
  (property name String))
(defnode Modifier
  (property name String))

(defn solo [ss] (or (first ss) (throw (ex-info (str "Exactly one result was expected. Got " (count ss)) {}))))

(defn q [w clauses] (solo (iq/query w clauses)))

(deftest scope-registration
  (testing "Nodes are registered within a scope by name"
    (with-clean-world
      (transactional
        (in (add (make-particle-editor :label "view scope"))
          (add (make-emitter :name "emitter"))
          (add (make-modifier :name "vortex"))))

      (let [scope-node (q world-ref [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q world-ref [[:name n]]))
                 "emitter"
                 "vortex")))))

(defnode DisposableNode
  t/IDisposable
  (dispose [this] (deliver (:latch this) true)))

(def gen-disposable-node
  (gen/fmap (fn [_] (make-disposable-node)) (gen/return 1)))

(def gen-nodelist
  (gen/vector gen-disposable-node))

(defspec scope-disposes-contained-nodes
  (prop/for-all [scoped-nodes gen-nodelist]
    (with-clean-world
      (let [scope          (transactional (add (make-scope)))
            disposables    (transactional (in scope (doseq [n scoped-nodes] (add n))) scoped-nodes)
            disposable-ids (map :_id disposables)]
        (transactional (delete scope))
        (let [last-tx   (:last-tx @world-ref)
              disposals (:values-to-dispose last-tx)]
          (is (= (sort disposable-ids) (sort (map :_id disposals)))))))))
