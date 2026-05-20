(ns hooks.clojure-core-cache
  (:require [clj-kondo.hooks-api :as api]))

(defn- binding-symbols [argv-node]
  (into []
        (comp
          (filter symbol?)
          (remove #(-> % name (.startsWith "_"))))
        (tree-seq coll? seq (api/sexpr argv-node))))

(defn- consume-bindings-node [argv-node body]
  (api/list-node
    (list*
      (api/token-node 'let)
      (api/vector-node
        [(api/token-node (gensym "_method_args__"))
         (api/vector-node
           (mapv api/token-node (binding-symbols argv-node)))])
      (if (seq body)
        body
        [(api/token-node nil)]))))

(defn- method-node [node]
  (let [[method-name-node argv-node & body] (:children node)]
    (if (vector? (api/sexpr argv-node))
      (api/list-node
        [method-name-node
         argv-node
         (consume-bindings-node argv-node body)])
      node)))

(defn- specific-node [node]
  (if (seq (:children node))
    (method-node node)
    node))

(defn defcache [{:keys [node]}]
  (let [[_ type-name-node fields-node & specifics] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'deftype)
         type-name-node
         fields-node
         (map specific-node specifics)))}))
