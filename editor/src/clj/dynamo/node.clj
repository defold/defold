(ns dynamo.node
  "This namespace has two fundamental jobs. First, it provides the operations
for defining and instantiating nodes: `defnode` and `construct`.

Second, this namespace defines some of the basic node types and mixins."
  (:require [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [dynamo.file :as file]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.system :as ds]
            [dynamo.types :as t :refer :all]
            [dynamo.util :refer :all]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.property :as ip]
            [internal.transaction :as it]
            [plumbing.core :as pc]))

(defn construct
  "Creates an instance of a node. The node-type must have been
  previously defined via `defnode`.

  The node's properties will all have their default values. The caller
  may pass key/value pairs to override properties.

  A node that has been constructed is not connected to anything and it
  doesn't exist in the graph yet. The caller must use
  `dynamo.system/add` to place the node in the graph.

  Example:
  (defnode GravityModifier
    (property acceleration t/Int (default 32))

  (construct GravityModifier :acceleration 16)"
  [node-type & {:as args}]
  (assert (::ctor node-type))
  ((::ctor node-type) (merge {:_id (in/tempid)} args)))

(defn abort
  "Abort production function and use substitute value."
  [msg & args]
  (throw (apply ex-info msg args)))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'."
  [node type & {:as body}]
  (gt/process-one-event (ds/refresh node) (assoc body :type type)))

;; ---------------------------------------------------------------------------
;; Intrinsics
;; ---------------------------------------------------------------------------
(defn- gather-property [this prop]
  (let [type     (-> this gt/properties prop)
        value    (get this prop)
        problems (t/property-validate type value)]
    {:node-id             (:_id this)
     :value               value
     :type                type
     :validation-problems problems}))

(pc/defnk gather-properties :- t/Properties
  "Production function that delivers the definition and value
for all properties of this node."
  [this]
  (let [property-names (-> this gt/properties keys)]
    (zipmap property-names (map (partial gather-property this) property-names))))

(defn- ->vec [x] (if (coll? x) (vec x) (if (nil? x) [] [x])))

(defn- resource?
  [node label]
  (some-> node gt/node-type gt/properties' label resource?))

(defn- resources-connected
  [transaction self prop]
  (let [graph (ds/in-transaction-graph transaction)]
    (vec (ig/sources graph (:_id self) prop))))

(defn lookup-node-for-filename
  [transaction parent self filename]
  (or
    (get-in transaction [:filename-index filename])
    (if-let [added-this-txn (first (filter #(= filename (:filename %)) (ds/transaction-added-nodes transaction)))]
      added-this-txn
      (t/lookup parent filename))))

(defn decide-resource-handling
  [transaction parent self surplus-connections prop project-path]
  (if-let [existing-node (lookup-node-for-filename transaction parent self project-path)]
    (if (some #{[(:_id existing-node) :content]} surplus-connections)
      [:existing-connection existing-node]
      [:new-connection existing-node])
    [:new-node nil]))

(defn remove-vestigial-connections
  [actions self prop surplus-connections]
  (into actions
        (for [[n l] surplus-connections]
          (it/disconnect {:_id n} l self prop))))

(defn- ensure-resources-connected
  [transaction parent self prop]
  (loop [actions             []
         project-paths       (map #(file/make-project-path parent %) (->vec (get self prop)))
         surplus-connections (resources-connected transaction self prop)]
    (if-let [project-path (first project-paths)]
      (let [[handling existing-node] (decide-resource-handling transaction parent self surplus-connections prop project-path)]
        (cond
          (= :new-node handling)
          (let [new-node (t/node-for-path parent project-path)]
            (recur
             (concat actions
                     [(it/new-node new-node)
                      (it/connect new-node :content self prop)
                      (it/connect new-node :self parent :nodes)])
             (next project-paths)
             surplus-connections))

          (= :new-connection handling)
          (recur
           (conj actions
                 (it/connect existing-node :content self prop))
           (next project-paths)
           surplus-connections)

          (= :existing-connection handling)
          (recur actions (next project-paths) (remove #{[(:_id existing-node) :content]} surplus-connections))))
      (concat actions
              (remove-vestigial-connections self prop surplus-connections)))))

(defn connect-resource
  [transaction graph self label kind properties-affected]
  (let [parent (ds/parent graph self)]
    (mapcat
     #(fn [prop]
        (when (resource? self prop)
          (ensure-resources-connected transaction parent self prop)))
     properties-affected)))

(def node-intrinsics
  [(list 'output 'self `t/Any `(pc/fnk [~'this] ~'this))
   (list 'output 'properties `t/Properties `gather-properties)])
