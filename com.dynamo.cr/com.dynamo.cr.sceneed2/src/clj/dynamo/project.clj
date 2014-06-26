(ns dynamo.project
  (:require [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [internal.graph.query :as q]))

(defonce project-state (ref {:graph (dg/empty-graph)}))

(defn new-resource
  ([resource]
    (new-resource resource (set (keys (:inputs resource))) (set (keys (:transforms resource)))))
  ([resource inputs outputs]
    [{:type :create-node
        :node resource
        :inputs inputs
        :outputs outputs}]))

(defn update-resource
  [resource f & args]
  [{:type :update-node
      :node-id (:_id resource)
      :fn f
      :args args}])

(defn connect
  [from-resource from-label to-resource to-label]
  [{:type :connect
     :source-id    (:_id from-resource)
     :source-label from-label
     :target-id    (:_id to-resource)
     :target-label to-label}])

(defn disconnect
  [from-resource from-label to-resource to-label]
  [{:type :disconnect
     :source-id    (:_id from-resource)
     :source-label from-label
     :target-id    (:_id to-resource)
     :target-label to-label}])

(defn resolve-tempid [ctx x] (if (pos? x) x (get (:tempids ctx) x)))

(defmulti perform (fn [ctx m] (:type m)))

(defmethod perform :create-node
  [{:keys [graph tempids] :as ctx} m]
  (let [next-id (dg/next-node graph)]
    (assoc ctx
      :graph (lg/add-labeled-node graph (:inputs m) (:outputs m) (assoc (:node m) :_id next-id))
      :tempids (assoc tempids (get-in m [:node :_id]) next-id))))

(defmethod perform :update-node
  [ctx m]
  (update-in ctx [:graph] #(apply dg/transform-node % (resolve-tempid ctx (:node-id m)) (:fn m) (:args m))))

(defmethod perform :connect
  [ctx m]
  (update-in ctx [:graph] lg/connect (resolve-tempid ctx (:source-id m)) (:source-label m) (resolve-tempid ctx (:target-id m)) (:target-label m)))

(defmethod perform :disconnect
  [ctx m]
  (update-in ctx [:graph] lg/disconnect (resolve-tempid ctx (:source-id m)) (:source-label m) (resolve-tempid ctx (:target-id m)) (:target-label m)))

(defn apply-tx
  [ctx actions]
  (reduce
    (fn [ctx action]
      (cond
        (sequential? action) (apply-tx ctx action)
        :else                (perform ctx action)))
    ctx
    actions))

(defn- new-transaction-context
  [g]
  {:graph g
   :tempids {}})

(defn transact
  [tx]
  (dosync
    (let [tx-result (apply-tx (new-transaction-context (:graph @project-state)) tx)]
      (alter project-state assoc :graph (:graph tx-result))
      (assoc tx-result :status :ok))))
