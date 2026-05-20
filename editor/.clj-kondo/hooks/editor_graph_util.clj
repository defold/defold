(ns hooks.editor-graph-util
  (:require [clj-kondo.hooks-api :as api]))

(defn- keyword-call-node [keyword-node pb-map-sym]
  (api/list-node
    [keyword-node
     (api/token-node pb-map-sym)]))

(defn- keyword-call-with-default-node [keyword-node pb-map-sym default-node]
  (api/list-node
    [keyword-node
     (api/token-node pb-map-sym)
     default-node]))

(defn- parsed-source-node [pb-map-sym source-node]
  (let [source (api/sexpr source-node)]
    (cond
      (keyword? source)
      (keyword-call-node source-node pb-map-sym)

      (seq? source)
      (let [[head-node second-node third-node & more] (:children source-node)
            head (api/sexpr head-node)
            second (api/sexpr second-node)]
        (cond
          (and (empty? more)
               (keyword? head)
               (= :or second))
          (keyword-call-with-default-node head-node pb-map-sym third-node)

          (and (nil? third-node)
               (symbol? head))
          (api/list-node
            [head-node
             (parsed-source-node pb-map-sym second-node)])

          :else
          source-node))

      :else
      source-node)))

(defn- set-property-node [node-id-sym pb-map-sym property-node source-node]
  (api/list-node
    [(api/token-node 'dynamo.graph/set-property)
     (api/token-node node-id-sym)
     (api/token-node (keyword (api/sexpr property-node)))
     (parsed-source-node pb-map-sym source-node)]))

(defn- set-property-nodes [node-id-sym pb-map-sym mapping-nodes]
  (mapv
    (fn [[property-node source-node]]
      (set-property-node node-id-sym pb-map-sym property-node source-node))
    (partition 2 mapping-nodes)))

(defn set-properties-from-pb-map [{:keys [node]}]
  (let [[_ node-id-node pb-class-node pb-map-node & mapping-nodes] (:children node)
        node-id-sym (gensym "node-id__")
        pb-map-sym (gensym "pb-map__")]
    {:node
     (api/list-node
       [(api/token-node 'let)
        (api/vector-node
          [(api/token-node node-id-sym)
           node-id-node
           (api/token-node (gensym "_pb-class__"))
           pb-class-node
           (api/token-node pb-map-sym)
           pb-map-node])
        (api/list-node
          (list*
            (api/token-node 'concat)
            (set-property-nodes node-id-sym pb-map-sym mapping-nodes)))] )}))
