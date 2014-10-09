(ns dynamo.node
  "Define new node types"
  (:require [internal.node :as in]
            [dynamo.types :refer :all]))

(set! *warn-on-reflection* true)

(defmacro transactional
  "Executes the body within a project transaction. All actions
described in the body will happen atomically at the end of the transactional
block.

Some special syntax is allowed in this form. These imperative clauses describe
the actions that should take place in the transaction.

(repaint)
Force a repaint of the current editor

(new _node-type_ & [_property-name_ _property-value ...])
Create a new node of the given type. Any properties listed here
will override the node's default values for those properties.

If you want to attach or detach connections to the new node, you must use
a \"tempid\". That is an ID with a negative value. For example:
   (new AtlasNode :_id -1)
   (new TextureCompiler :_id -2)
   (connect {:_id -1} :textureset {:_id -2} :textureset)

Anywhere that the same tempid appears in the same transaction, it refers to the
same node. Tempids are replaced with real IDs when the transaction executes.

(attach _from-node_ _from-label_ _to-node_ _to-label_)
Connect the output _from-label_ to the input _to-label_ on the given nodes.

(detach _from-node_ _from-label_ _to-node_ _to-label_)
The opposite of attach.

(set-property _node_ _property_ _value_ [_property-2_ _value-2_ ...])
Assign a value to the node's property. Note that this only takes effect at the end
of the transaction, so the new value will not be visible to anyone during this body.

(update-property _node_ _property_ _function_ & [_args])
Apply a function to the a property of a node. The function is called with
the current value of the property and any additional arguments you supply.

(send _node_ _msg-type_ & _body_)
Send a message to a node. If the node does not have a processor for that message
type, it will complain in the system log, but otherwise ignore the message."
  [& forms]
  `(in/transactional ~@forms))

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
an AtlasCompiler to the project. `defnode` merges the behaviors appropriately.

Every node can receive messages. The node declares message handlers with a special syntax:

(on _message_type_ _form_)

The form will be evaluated inside a transactional body. This means that it can
use the special clauses to create nodes, change connections, update properties, and so on.

A node definition allows any number of 'on' clauses.

A node may also implement protocols or interfaces, using a syntax identical
to `deftype` or `defrecord`. A node may implement any number of such protocols.

Every node always implements dynamo.types/Node.

If there are any event handlers defined for the node type, then it will also
implement dynamo.types/MessageTarget."
  [name & specs]
  (let [beh (in/compile-specification name specs)]
    `(do
       ~(in/generate-type name beh)
       ~(in/generate-constructor name beh)
       ~(in/generate-print-method name)
       ~(in/generate-descriptor name beh))))

(defn dispatch-message
  "This is an advanced usage. If you have a reference to a node, you can directly send
it a message.

This function should mainly be used to create 'plumbing'. In most cases you will want
to use dynamo.project/publish to send a message to a node."
  [node type & {:as body}]
  (process-one-event node (assoc body :type type)))
