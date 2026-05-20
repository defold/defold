(ns hooks.editor-ui
  (:require [clj-kondo.hooks-api :as api]))

(defn- child-key [child-node]
  (keyword (api/sexpr child-node)))

(defn- child-id [child-node]
  (str (api/sexpr child-node)))

(defn- keyword-call-node [keyword controls-sym]
  (api/list-node
    [(api/token-node keyword)
     (api/token-node controls-sym)]))

(defn- collect-controls-node [parent-node child-nodes]
  (api/list-node
    [(api/token-node 'editor.ui/collect-controls)
     parent-node
     (api/vector-node
       (mapv #(api/token-node (child-id %)) child-nodes))]))

(defn- control-binding-nodes [controls-sym child-nodes]
  (into []
        (mapcat
          (fn [child-node]
            [child-node
             (keyword-call-node (child-key child-node) controls-sym)]))
        child-nodes))

(defn with-controls [{:keys [node]}]
  (let [[_ parent-node children-node & body] (:children node)
        child-nodes (:children children-node)
        controls-sym (gensym "controls__")]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node
           (into [(api/token-node controls-sym)
                  (collect-controls-node parent-node child-nodes)]
                 (control-binding-nodes controls-sym child-nodes)))
         body))}))

(defn- body-node [body]
  (api/list-node
    (list*
      (api/token-node 'let)
      (api/vector-node
        [(api/token-node (gensym "_this__"))
         (api/token-node 'this)])
      body)))

(defn- reify-node [class-sym method-sym argv-nodes body]
  (api/list-node
    [(api/token-node 'reify)
     (api/token-node class-sym)
     (api/list-node
       [(api/token-node method-sym)
        (api/vector-node argv-nodes)
        (body-node body)])]))

(defn event-handler [{:keys [node]}]
  (let [[_ event-node & body] (:children node)]
    {:node
     (reify-node
       'javafx.event.EventHandler
       'handle
       [(api/token-node 'this) event-node]
       body)}))

(defn event-dispatcher [{:keys [node]}]
  (let [[_ event-node tail-node & body] (:children node)]
    {:node
     (reify-node
       'javafx.event.EventDispatcher
       'dispatchEvent
       [(api/token-node 'this) event-node tail-node]
       body)}))

(defn change-listener [{:keys [node]}]
  (let [[_ observable-node old-val-node new-val-node & body] (:children node)]
    {:node
     (reify-node
       'javafx.beans.value.ChangeListener
       'changed
       [(api/token-node 'this) observable-node old-val-node new-val-node]
       body)}))

(defn invalidation-listener [{:keys [node]}]
  (let [[_ observable-node & body] (:children node)]
    {:node
     (reify-node
       'javafx.beans.InvalidationListener
       'invalidated
       [(api/token-node 'this) observable-node]
       body)}))

(defn- progress-done-node [bindings-node]
  (api/list-node
    [(api/list-node
       [(api/token-node 'second)
        bindings-node])
     (api/token-node 'editor.progress/done)]))

(defn- try-finally-node [body finally-node]
  (api/list-node
    (concat
      [(api/token-node 'try)]
      body
      [(api/list-node
         [(api/token-node 'finally)
          finally-node])])))

(defn with-progress [{:keys [node]}]
  (let [[_ bindings-node & body] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'let)
        bindings-node
        (try-finally-node body (progress-done-node bindings-node))])}))
