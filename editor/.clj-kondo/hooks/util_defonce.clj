(ns hooks.util-defonce
  (:require [clj-kondo.hooks-api :as api]))

(defn protocol [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'defprotocol)
         name-node
         body))}))

(defn record [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'defrecord)
         name-node
         body))}))

(defn type [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'deftype)
         name-node
         body))}))

(defn interface [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'definterface)
         name-node
         body))}))
