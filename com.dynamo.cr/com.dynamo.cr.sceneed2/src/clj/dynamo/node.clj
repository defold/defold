(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [internal.node :as in]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [service.log :as log]
            [dynamo.types :refer :all]))

(set! *warn-on-reflection* true)

(defn get-node [g id]
  (dg/node g id))

(defmacro defnode
  [name & behaviors]
  (let [behavior (in/merge-behaviors behaviors)]
    `(let []
       ~(in/generate-type name behavior)
       ~(in/generate-constructor name behavior))))

(defmacro defnode
  "Given a name and a specification of behaviors, creates a node
and attendant functions.

A record is created from name, along with a constructor of the
form `make-node-name` that creates an instance of the node record
type and adds it to the project.

Allowed clauses are:

(inherit  _symbol_)
Compose the behavior from the named node type

(input    _symbol_ _schema_)
Define an input with the name, whose values must match the schema.

(property _symbol_ _property-spec_)
Define a property with schema and, possibly, default value and constraints.

(output   _symbol_ _type_ (:cached)? (:on-update)? _producer_)
Define an output to produce values of type. Flags ':cached' and
':on-update' are optional. _producer_ may be a var that names an fn, or fnk.
It may also be a function tail as [arglist] + forms.

Example (from [[atlas.core]]):

    (defnode TextureCompiler
      (input    textureset (as-schema TextureSet))
      (property texture-filename (string :default \"\"))
      (output   texturec s/Any compile-texturec)))

    (defnode TextureSetCompiler
      (input    textureset (as-schema TextureSet))
      (property textureset-filename (string :default \"\"))
      (output   texturesetc s/Any compile-texturesetc)))

    (defnode AtlasCompiler
      (inherit TextureCompiler)
      (inherit TextureSetCompiler))

This will produce a record `AtlasCompiler`, with a constructor function `make-atlas-compiler`, for adding
an AtlasCompiler to the project. `defnode` merges the behaviors appropriately."
  [name & specs]
  (let [beh (in/compile-specification name specs)]
    `(do
       ~(in/generate-type name beh)
       ~(in/generate-constructor name beh)
       ~(in/generate-descriptor name beh))))

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
          }]
  (alter-meta! v assoc :doc doc))
