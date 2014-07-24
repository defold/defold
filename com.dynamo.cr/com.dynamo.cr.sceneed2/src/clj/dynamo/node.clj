(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [internal.node :as in]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [dynamo.types :refer :all]))

(declare get-value)

(defn get-node [g id]
  (dg/node g id))

(defn get-inputs [target-node g target-label]
  (let [schema (get-in target-node [:inputs target-label])]
    (if (vector? schema)
      (map (fn [[source-node source-label]]
             (get-value g (dg/node g source-node) source-label))
           (lg/sources g (:_id target-node) target-label))
      (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
        (get-value g (dg/node g first-source-node) first-source-label)))))

(defn collect-inputs [node g input-schema seed]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        :project   m
        s/Keyword  m
        (assoc m k (get-inputs node g k))))
    seed input-schema))

(defn- perform [transform node g seed]
  (cond
    (symbol?     transform)  (perform (resolve transform) node g seed)
    (var?        transform)  (perform (var-get transform) node g seed)
    (has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform) seed))
    (fn?         transform)  (transform node g)
    :else transform))

(defn get-value [g node label & [seed]]
  (perform (get-in node [:transforms label]) node g seed))

(defn add-node [g node]
  (lg/add-labeled-node g (in/node-inputs node) (in/node-outputs node) node))

(defmacro defnode
  "given a name and a list of behaviors, creates a node and attendant functions.

A record is created from name, along with a constructor of the form `make-node-name` that creates an instance
of the node record type and adds it to the project.

Example (from [[atlas.core]]):

    (def TextureCompiler
      {:inputs     {:textureset (as-schema TextureSet)}
       :properties {:texture-filename {:schema s/Any :default \"\"}}
       :transforms {:texturec    #'compile-texturec}
       :on-update  #{:texturec}})

    (def TextureSetCompiler
      {:inputs     {:textureset (as-schema TextureSet)}
       :properties {:textureset-filename {:schema s/Any :default \"\"}}
       :transforms {:texturesetc #'compile-texturesetc}
       :on-update  #{:texturesetc}})

    (defnode AtlasCompiler
      TextureCompiler
      TextureSetCompiler)

This will produce a record `AtlasCompiler`, with a constructor function `make-atlas-compiler`, for adding
an AtlasCompiler to the project. `defnode` merges the behaviors appropriately."
  [name & behaviors]
  (let [behavior (in/merge-behaviors behaviors)]
    `(let []
       ~(in/generate-type name behavior)
       ~@(mapcat in/input-mutators (keys (:inputs behavior)))
       ~(in/generate-constructor name behavior))))


(doseq [[v doc]
       {*ns*
        "API methods for interacting with nodes---elements within the project graph.

A node is expected to be a map as follows:

    { :properties { } ; map of properties
      :transforms { } ; map of transform label (key)
                      ; to function symbol (value)
      :inputs     #{} ; set of input labels (keywords)
      :outputs    #{} ; set of output labels (keywords)
    }"

        #'add-node
        "adds node `node` to graph `g`. Node is a map, optionally including sets of inputs and outputs as `:inputs` and `:outputs`, respectively."

        #'collect-inputs
        "produces a map of inputs sufficient to satisfy the `input-schema`."

        #'get-inputs
        "given a graph, node, and an input label on the node returns the value or values provided to that input via its connections."

        #'get-value
        "given a graph, node, and transform label, `get-value` returns the result of the transform."
          }]
  (alter-meta! v assoc :doc doc))
