(ns internal.scope-test
  (:require [clojure.core.async :as a]
            [dynamo.node :refer :all]
            [dynamo.system.test-support :refer :all]
            [dynamo.system :as ds :refer [transactional add in]]
            [dynamo.types :as t]
            [internal.query :as iq]
            [clojure.test :refer :all]))

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

      (def *the-world @world-ref)
      (let [scope-node (q world-ref [[:label "view scope"]])]
        (are [n] (identical? (t/lookup scope-node n) (q world-ref [[:name n]]))
                 "emitter"
                 "vortex")))))
