(ns internal.node
  (:require [clojure.pprint :refer [print-table]]
            [clojure.set :refer [rename-keys]]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [camel-snake-kebab :refer [->kebab-case]]))

(defn node-inputs [v] (into #{} (keys (:inputs v))))
(defn node-outputs [v] (into #{} (keys (:transforms v))))

(defn get-input [])
(defn refresh-inputs [g n i])


(defprotocol Node
  (value [this g output default] "Produce the value for the named output. Supplies nil if the output cannot be produced.")
  (properties [this] "Produce a description of properties supported by this node."))

(defn merge-behaviors [behaviors]
  (apply (partial merge-with merge)
         (map #(cond 
                 (symbol? %) (var-get (resolve %))
                 (var? %)    (var-get (resolve %))
                 (map? %)    %
                 :else       (throw (ex-info (str "Unacceptable behavior " %) :argument %)))
              behaviors)))

(defn- property-symbols [behavior]
  (map (comp symbol name) (keys (:properties behavior))))

(defn state-vector [behavior]
  (into [] (list* 'inputs 'transforms  (property-symbols behavior))))

(defn generate-type [name behavior]
  (let [t 'this#
        g 'g#
        o 'output#
        d 'default#]
    (list 'defrecord name (state-vector behavior)
          'internal.node/Node
          `(value [~t ~g ~o ~d]
                  (cond
                    ~@(mapcat (fn [x] [(list '= o x) (list 'prn x)]) (keys (:outputs behavior)))
                    :else ~d))
          `(properties [~t]
                       ~(:properties behavior)))))

(defn wire-up [selection input new-node output]
  (let [g           (first selection)
        target-node (first (second selection))
        g-new       (lg/add-labeled-node g (node-inputs new-node) (node-outputs new-node) new-node)
        new-node-id (dg/last-node-added g-new)]
      (-> g
        (lg/connect new-node-id output target-node input)
        (refresh-inputs target-node input))))

(defn unwire [selection input source]
  (let [g           (first selection)
        target-node (first (second selection))
        g-new       (dg/for-graph g [l (lg/source-labels g target-node :input source)]
                                  (lg/disconnect g source l target-node input))]
    (refresh-inputs g target-node input)))

(defn input-mutators [input]
  (let [adder (symbol (str "add-to-" (name input)))
        remover (symbol (str "remove-from-" (name input)))]
    (list
      `(defn ~adder [~'selection ~'new-node ~'output]
         (wire-up ~'selection ~input ~'new-node ~'output))
      `(defn ~remover [~'selection ~'new-node]
         (unwire ~'selection ~input ~'new-node)))))

(defn defaults [behavior]
  (reduce-kv (fn [m k v] (if (:default v) (assoc m k (:default v)) m))
             behavior (:properties behavior)))

(defn describe-properties [behavior]
  (with-out-str (print-table ["Name" "Type" "Default"] 
                            (reduce-kv 
                              (fn [rows k v] 
                                (conj rows (assoc (rename-keys v {:schema "Type" :default "Default"}) "Name" k))) 
                              [] 
                              (:properties behavior)))))

(defn generate-constructor [nm behavior]
  (let [ctor             (symbol (str 'make- (->kebab-case (str nm))))
        record-ctor      (symbol (str 'map-> nm))]
    `(defn ~ctor
       ~(str "Constructor for " nm ", using default values for any property not in property-values.\nThe properties on " nm " are:\n"
             (describe-properties behavior))
       [& {:as ~'property-values}]
       (~record-ctor (merge ~behavior ~(defaults behavior) ~'property-values)))))
