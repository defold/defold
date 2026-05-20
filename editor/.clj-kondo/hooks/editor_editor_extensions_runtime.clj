(ns hooks.editor-editor-extensions-runtime
  (:require [clj-kondo.hooks-api :as api]))

(defn- fn-node [node]
  (let [[_ & fn-tail] (:children node)]
    (api/list-node
      (list*
        (api/token-node 'clojure.core/fn)
        fn-tail))))

(defn lua-fn [{:keys [node]}]
  {:node (fn-node node)})

(defn suspendable-lua-fn [{:keys [node]}]
  {:node (fn-node node)})

(defn suspendable-varargs-lua-fn [{:keys [node]}]
  {:node (fn-node node)})

(defn varargs-lua-fn [{:keys [node]}]
  {:node (fn-node node)})
