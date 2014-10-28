(ns dynamo.system.test-support
  (:require [dynamo.node :as n]
            [dynamo.system :as ds :refer [in]]
            [internal.query :as iq]
            [internal.system :as is]
            [internal.transaction :as it]))

(set! *warn-on-reflection* true)

(defn clean-world
  []
  (let [world-ref (ref nil)
        root      (n/make-root :world-ref world-ref :_id 1)]
    (dosync
      (ref-set world-ref (is/new-world-state world-ref root))
      world-ref)))

(defmacro with-clean-world
  [& forms]
  `(let [~'world-ref (clean-world)
         ~'root      (iq/node-by-id ~'world-ref 1)]
     (ds/in ~'root
         ~@forms)))

(defn tx-nodes [world-ref & resources]
  (ds/transactional
    (doseq [r resources]
      (ds/add r))
    resources))