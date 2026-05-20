(ns hooks.editor-gui
  (:require [clj-kondo.hooks-api :as api]))

(defn- partial-node [& args]
  (api/list-node
    (mapv api/token-node
          (list* 'fn/partial args))))

(defn- expr-node [x]
  (if (map? x)
    x
    (api/token-node x)))

(defn- assoc-node [edit-type-node & kvs]
  (api/list-node
    (list*
      (api/token-node 'assoc)
      edit-type-node
      (map expr-node kvs))))

(defn wrap-layout-property-edit-type [{:keys [node]}]
  (let [[_ prop-node edit-type-node changes-fn-node] (:children node)
        prop-kw (keyword (api/sexpr prop-node))]
    {:node
     (if changes-fn-node
       (assoc-node
         edit-type-node
         :set-fn
         (partial-node 'layout-property-edit-type-set-in-current-layout
                       (api/sexpr changes-fn-node)
                       prop-kw)
         :clear-fn
         (partial-node 'layout-property-edit-type-clear-in-current-layout
                       (api/sexpr changes-fn-node))
         :changes-fn
         (api/sexpr changes-fn-node))
       (assoc-node
         edit-type-node
         :set-fn
         (partial-node 'layout-property-edit-type-set-in-current-layout nil prop-kw)
         :clear-fn
         'basic-layout-property-clear-in-current-layout))}))

(defn- outline-children-node [sort-children-node]
  (if (api/sexpr sort-children-node)
    (api/list-node
      [(api/token-node 'vec)
       (api/list-node
         [(api/token-node 'sort-by)
          (api/token-node :child-index)
          (api/token-node 'child-outlines)])])
    (api/token-node 'child-outlines)))

(defn gen-outline-fnk [{:keys [node]}]
  (let [[_ label-node node-outline-key-node order-node sort-children-node child-reqs-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'dynamo.graph/fnk)
        (api/vector-node
          [(api/token-node '_node-id)
           (api/token-node 'child-outlines)])
        (api/list-node
          [(api/token-node 'hash-map)
           (api/token-node :node-id)
           (api/token-node '_node-id)
           (api/token-node :node-outline-key)
           node-outline-key-node
           (api/token-node :label)
           label-node
           (api/token-node :icon)
           (api/token-node 'virtual-icon)
           (api/token-node :order)
           order-node
           (api/token-node :read-only)
           (api/token-node true)
           (api/token-node :child-reqs)
           child-reqs-node
           (api/token-node :children)
           (outline-children-node sort-children-node)])])}))
