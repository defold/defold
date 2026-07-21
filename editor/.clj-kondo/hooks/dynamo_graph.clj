;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns hooks.dynamo-graph
  (:require [clj-kondo.hooks-api :as api]))

(defn- split-argv [argv-node]
  (let [argv (vec (api/sexpr argv-node))
        argv-count (count argv)]
    (if (and (<= 2 argv-count)
             (= :as (nth argv (- argv-count 2))))
      {:deps (subvec argv 0 (- argv-count 2))
       :alias (peek argv)}
      {:deps argv})))

(defn- get-node [map-sym dep]
  (api/list-node
    [(api/token-node 'get)
     (api/token-node map-sym)
     (api/token-node (keyword dep))]))

(defn- alias-map-node [deps]
  (api/list-node
    (into [(api/token-node 'hash-map)]
          (mapcat
            (fn [dep]
              [(api/token-node (keyword dep))
               (api/token-node dep)]))
          deps)))

(defn- binding-nodes [map-sym deps alias]
  (cond-> (into []
                (mapcat
                  (fn [dep]
                    [(api/token-node dep)
                     (get-node map-sym dep)]))
                deps)
          alias
          (into [(api/token-node alias)
                 (alias-map-node deps)])))

(defn- fn-body-node [map-sym argv-node body]
  (let [{:keys [deps alias]} (split-argv argv-node)]
    (api/list-node
      (list*
        (api/token-node 'let)
        (api/vector-node (binding-nodes map-sym deps alias))
        (if (seq body)
          body
          [(api/token-node nil)])))))

(defn fnk [{:keys [node]}]
  (let [[_ argv-node & body] (:children node)
        map-sym (gensym (if (seq (api/sexpr argv-node))
                          "fnk-map__"
                          "_fnk-map__"))]
    {:node
     (api/list-node
       [(api/token-node 'fn)
        (api/vector-node [(api/token-node map-sym)])
        (fn-body-node map-sym argv-node body)])}))

(defn- vector-node? [node]
  (vector? (api/sexpr node)))

(defn defnk [{:keys [node]}]
  (let [[_ name-node & tail] (:children node)
        [preamble [argv-node & body]] (split-with (complement vector-node?) tail)
        map-sym (gensym (if (seq (api/sexpr argv-node))
                          "fnk-map__"
                          "_fnk-map__"))]
    {:node
     (api/list-node
       (concat
         [(api/token-node 'defn)
          name-node]
         preamble
         [(api/vector-node [(api/token-node map-sym)])
          (fn-body-node map-sym argv-node body)]))}))

(defn- make-node-let-bindings [graph-id-sym binding-node]
  (into []
        (mapcat
          (fn [[local-node _rhs-node]]
            [local-node
             (api/token-node graph-id-sym)]))
        (partition 2 (:children binding-node))))

(defn- make-node-rhs-nodes [binding-node]
  (mapv second (partition 2 (:children binding-node))))

(defn- make-node-rhs-bindings [binding-node]
  (into []
        (mapcat
          (fn [rhs-node]
            [(api/token-node (gensym "_make-nodes-rhs__"))
             rhs-node]))
        (make-node-rhs-nodes binding-node)))

(defn- concat-node [body]
  (api/list-node
    (list*
      (api/token-node 'concat)
      body)))

(defn make-nodes [{:keys [node]}]
  (let [[_ graph-id-node binding-node & body] (:children node)
        graph-id-sym (gensym "graph-id__")]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node
           (into [(api/token-node graph-id-sym)
                  graph-id-node]
                 (concat
                   (make-node-let-bindings graph-id-sym binding-node)
                   (make-node-rhs-bindings binding-node))))
         (if (seq body)
           [(concat-node body)]
           [(api/token-node graph-id-sym)])))}))

(defn with-auto-evaluation-context [{:keys [node]}]
  (let [[_ evaluation-context-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node
           [evaluation-context-node
            (api/list-node
              [(api/token-node 'dynamo.graph/make-evaluation-context)])])
         body))}))

(defn with-auto-or-fake-evaluation-context [{:keys [node]}]
  (with-auto-evaluation-context {:node node}))

(defn- evaluation-context-node []
  (api/list-node
    [(api/token-node 'dynamo.graph/make-evaluation-context)]))

(defn- symbols-in-node [node]
  (into #{}
        (filter symbol?)
        (tree-seq coll? seq (api/sexpr node))))

(defn- let-ec-init-node [init-node]
  (api/list-node
    [(api/token-node 'let)
     (api/vector-node
       [(api/token-node 'evaluation-context)
        (evaluation-context-node)
        (api/token-node '_evaluation-context-used-by-clj-kondo-hook)
        (api/token-node 'evaluation-context)])
     init-node]))

(defn- let-ec-binding-nodes [binding-node]
  (into []
        (mapcat
          (fn [[binding-form-node init-node]]
            [binding-form-node
             (let-ec-init-node init-node)]))
        (partition 2 (:children binding-node))))

(defn let-ec [{:keys [node]}]
  (let [[_ binding-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node (let-ec-binding-nodes binding-node))
         body))}))

(defn- call-node? [node]
  (seq (:children node)))

(defn- call-head [node]
  (when (call-node? node)
    (api/sexpr (first (:children node)))))

(defn- fn-like-node? [node]
  (let [head (call-head node)]
    (contains? '#{fn fn* clojure.core/fn dynamo.graph/fnk g/fnk
                  dynamo.graph/constantly g/constantly
                  editor.gui/gen-outline-fnk gen-outline-fnk}
               head)))

(defn- lintable-function-node? [node]
  (or (symbol? (api/sexpr node))
      (fn-like-node? node)))

(defn- defnode-labels [body]
  (into '#{_declared-properties _evaluation-context _node-id _output-jammers _properties _this}
        (keep
          (fn [form-node]
            (when (call-node? form-node)
              (case (call-head form-node)
                input (api/sexpr (second (:children form-node)))
                output (api/sexpr (second (:children form-node)))
                property (api/sexpr (second (:children form-node)))
                nil))))
        body))

(defn- defnode-label-bindings [labels]
  (into []
        (mapcat
          (fn [label]
            [(api/token-node label)
             (api/list-node
               [(api/token-node 'throw)
                (api/list-node
                  [(api/token-node 'ex-info)
                   (api/token-node "lint-only defnode label")
                   (api/map-node [])])])]))
        labels))

(defn- expression-node [node]
  (api/list-node
    [(api/token-node 'identity)
     node]))

(defn- unknown-value-node []
  (api/list-node
    [(api/token-node 'throw)
     (api/list-node
       [(api/token-node 'ex-info)
        (api/token-node "lint-only graph value")
        (api/map-node [])])]))

(defn- scoped-expression-node [labels nodes]
  (let [used-labels (into #{}
                          (filter labels)
                          (mapcat symbols-in-node nodes))
        expression-nodes (map expression-node nodes)]
    (if (seq used-labels)
      (api/list-node
        (list*
          (api/token-node 'let)
          (api/vector-node (defnode-label-bindings used-labels))
          expression-nodes))
      (api/list-node
        (list*
          (api/token-node 'do)
          expression-nodes)))))

(defn deftype [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'do)
         (api/list-node
           [(api/token-node 'def)
            name-node
            (unknown-value-node)])
         (map expression-node body)))}))

(defn- output-scoped-nodes [form-node]
  (let [[_ _ _ & tail] (:children form-node)
        tail (drop-while #(keyword? (api/sexpr %)) tail)
        function-node (first tail)]
    (when (and function-node
               (not= :abstract (api/sexpr function-node))
               (not (lintable-function-node? function-node)))
      [function-node])))

(defn- output-unscoped-nodes [form-node]
  (let [[_ _ _ & tail] (:children form-node)
        tail (drop-while #(keyword? (api/sexpr %)) tail)
        function-node (first tail)]
    (when (and function-node
               (not= :abstract (api/sexpr function-node))
               (lintable-function-node? function-node))
      [function-node])))

(defn- input-option-nodes [form-node]
  (let [[_ _ _ & tail] (:children form-node)]
    (loop [nodes []
           tail tail]
      (if-let [[option-node & tail] (seq tail)]
        (let [value-node (first tail)]
          (if (and (keyword? (api/sexpr option-node))
                   value-node
                   (not (keyword? (api/sexpr value-node))))
            (recur (conj nodes value-node) (rest tail))
            (recur nodes tail)))
        nodes))))

(defn- property-option-nodes [form-node]
  (into []
        (keep
          (fn [option-node]
            (when (call-node? option-node)
              (let [[head-node & args] (:children option-node)]
                (case (api/sexpr head-node)
                  default (first args)
                  set (first args)
                  value (first args)
                  dynamic (second args)
                  nil)))))
        (drop 3 (:children form-node))))

(defn- property-scoped-option-nodes [form-node]
  (into []
        (remove lintable-function-node?)
        (property-option-nodes form-node)))

(defn- property-unscoped-option-nodes [form-node]
  (into []
        (filter lintable-function-node?)
        (property-option-nodes form-node)))

(defn- defnode-scoped-nodes [form-node]
  (when (call-node? form-node)
    (case (call-head form-node)
      input (input-option-nodes form-node)
      output (output-scoped-nodes form-node)
      property (property-scoped-option-nodes form-node)
      nil)))

(defn- defnode-unscoped-nodes [form-node]
  (when (call-node? form-node)
    (case (call-head form-node)
      inherits (rest (:children form-node))
      display-order [(second (:children form-node))]
      input [(nth (:children form-node) 2)]
      output (into [(nth (:children form-node) 2)]
                   (output-unscoped-nodes form-node))
      property (into [(nth (:children form-node) 2)]
                     (property-unscoped-option-nodes form-node))
      nil)))

(defn defnode [{:keys [node]}]
  (let [[_ name-node & body] (:children node)
        labels (defnode-labels body)
        scoped-nodes (mapcat defnode-scoped-nodes body)
        unscoped-nodes (mapcat defnode-unscoped-nodes body)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (api/list-node
          [(api/token-node 'def)
           name-node
           (unknown-value-node)])
        (scoped-expression-node labels scoped-nodes)
        (api/list-node
          (list*
            (api/token-node 'do)
            (map expression-node unscoped-nodes)))])}))
