(ns hooks.kondo-hooks
  (:require [clj-kondo.hooks-api :as api]))

(defn with-auto-evaluation-context [{:keys [node]}]
  (let [[_ binding & body] (:children node)
        new-node (api/list-node
                  (list*
                   (api/token-node 'let)
                   (api/vector-node [binding (api/token-node 'nil)])
                   body))]
    {:node new-node}))

(defn defc [{:keys [node]}]
  (let [[_ name-node attr-map params & body] (:children node)
        ;; Inject 'props' into the parameter destructuring
        params-with-props (if (api/vector-node? params)
                            (let [children (:children params)]
                              (if (and (seq children)
                                       (api/map-node? (first children)))
                                ;; Add props to the :keys if it's a map destructure
                                (api/vector-node
                                 [(api/map-node
                                   (concat (:children (first children))
                                           [(api/keyword-node :props)
                                            (api/token-node 'props)]))])
                                params))
                            params)
        new-node (api/list-node
                  (list*
                   (api/token-node 'defn)
                   name-node
                   params-with-props
                   body))]
    {:node new-node}))

(defn defnode [{:keys [node]}]
  (let [[_ name & clauses] (:children node)
        clause-forms (for [clause clauses
                          :when (api/list-node? clause)]
                      (let [[clause-type & args] (:children clause)]
                        (case (api/sexpr clause-type)
                          (property input) 
                          ;; (property name Type ...) -> (def name Type)
                          (let [prop-name (first args)
                                type-node (second args)]
                            (api/list-node 
                             [(api/token-node 'def) 
                              prop-name
                              type-node]))
                          
                          output
                          ;; (output name Type [:cached] producer) -> (def name (fn [] Type producer))
                          (let [prop-name (first args)
                                type-node (second args)
                                remaining (drop 2 args)
                                ;; Skip :cached if present
                                producer-forms (if (and (api/keyword-node? (first remaining))
                                                       (= :cached (api/sexpr (first remaining))))
                                                (rest remaining)
                                                remaining)]
                            (api/list-node 
                             [(api/token-node 'def) 
                              prop-name
                              (api/list-node 
                               (list* (api/token-node 'fn)
                                      (api/vector-node [])
                                      type-node
                                      producer-forms))]))
                          
                          ;; Default: keep as-is
                          clause)))
        new-node (api/list-node
                  (list*
                   (api/token-node 'do)
                   (api/list-node [(api/token-node 'def) name (api/token-node 'nil)])
                   clause-forms))]
    {:node new-node}))
