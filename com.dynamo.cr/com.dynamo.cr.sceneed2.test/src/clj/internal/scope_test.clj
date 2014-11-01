(ns internal.scope-test
  (:require [clojure.core.async :as a]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [schema.macros :as sm]
            [plumbing.core :refer [defnk]]
            [dynamo.node :refer :all]
            [dynamo.project :refer [Project make-project]]
            [dynamo.system.test-support :refer :all]
            [dynamo.system :as ds :refer [transactional add in]]
            [dynamo.types :as t]
            [internal.query :as iq]
            [internal.node :as in])
  (:import [dynamo.types Image AABB]))

(deftest input-compatibility
  (are [out out-type in in-type expect-compat why] (= expect-compat (in/compatible? out out-type in in-type))
    :image Image    :image  AABB      false    "type mismatch"
    :image Image    :image  Image     true     "ok"
    :image Image    :images [Image]   true     "ok"
    :image Image    :images Image     false    "plural name, singular type"
    :name  String   :names  [String]  true     "ok"
    :name  String   :names  String    false    "plural name, singular type"
    :names [String] :names  [String]  true     "ok"
    :name  String   :name   [String]  false    "singular name, plural type"
    ))


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
      (transactional world-ref
        (in (add (make-particle-editor :label "view scope"))
          (add (make-emitter :name "emitter"))
          (add (make-modifier :name "vortex"))))

      (let [scope-node (q world-ref [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q world-ref [[:name n]]))
                 "emitter"
                 "vortex")))))

(sm/defrecord CommonValueType
  [identifier :- String])

(defnk concat-all :- CommonValueType
  [values :- [CommonValueType]]
  (apply str/join (map :identifier values)))

(defnode ValueConsumer
  (input values [CommonValueType])
  (output concatenation CommonValueType concat-all))

(defnk passthrough [local-name] local-name)

(defnode InjectionScope
  (inherits Project)
  (input local-name CommonValueType)
  (output passthrough CommonValueType passthrough))

(defnode ValueProducer
  (property value {:schema CommonValueType})
  (output output-name CommonValueType [this _] (:value this)))

(deftest dependency-injection
  (testing "attach node output to input on scope"
    (with-clean-world
      (let [scope (ds/transactional
                    (ds/in (ds/add (make-injection-scope))
                      (ds/add (make-value-producer :value (CommonValueType. "a known value")))
                      (ds/current-scope)))]
        (is (= "a known value" (get-node-value scope :passthrough))))))
  (testing "using a project as scope"
    (with-clean-world
      (let [consumer (ds/transactional
                       (ds/in (ds/add (make-project))
                         (ds/add (make-value-producer :value (CommonValueType. "first")))
                         (ds/add (make-value-consumer))))]
        (is (= "first" (get-node-value consumer :concatenation)))))))

(run-tests)