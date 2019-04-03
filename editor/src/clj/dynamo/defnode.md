From g/defnode to g/node-value
==============================

Overview
--------

The `g/defnode` macro is used for defining new node types: the inputs,
properties and outputs of corresponding nodes, internal dependencies
and meta data about the node type.

Every node instance in the system has a reference to a certain node
type, and this node type is consulted whenever you do `g/node-value`
on the node, or when f.i. the output of a node is used as input to
another node we're doing `g/node-value` on. There are also
introspection functions that allow us to query a node f.i. if it has a
certain output or in what order it's properties should be displayed.

`g/defnode` (which should maybe be called `g/defnode_type_`) is
comlicated for a number of reasons, but foremost:

* We want to be able to reload node definitions and have the changes
  apply immediately to already existing nodes.
* We want `g/node-value` to be as efficient as possible.

The reloading is handled by an indirection between the node instances
and corresponding node type. The node type of a node instance is not
an assoc'ed immutable value, but rather a reference that when deref'd
will look up the actual "current revision" of the node type in a
global registry of node types. `g/defnode` will emit code to update
this registry.

The efficiency of `g/node-value` is acheived by having `g/defnode`
inspect the dependencies (arguments) of all production functions in
the node definition and generate code at compile time (macro expand
time) for argument collection, error checking, schema validation
etc. and finally calling the actual production function. Essentially
every production function will become a `def`'d function in the
namespace of the node definition, and `g/node-value` just dispatches
to that. Initial implementations of the graph system did all this at
runtime by inspecting the node type information but this was too
inefficient.

Where is all of this?
---------------------

So we have a couple of concepts now, where are they in the code?

* `g/defnode` is in `/src/clj/dynamo/graph.clj` (`dynamo.graph`), but the
  actual meat of it - which will be discussed shortly - is in
  `process-node-type-forms` in `/src/clj/internal/node.clj` (`internal.node`)
* The node types are all `NodeTypeImpl` records, definition in `internal.node`
* For node instances there are two record types: for normal nodes
  there is `NodeImpl` and for override nodes there is `OverrideNode` -
  both of these in `internal.node`. The concept of overrides will not be
  covered here, but we'll touch on some of `OverrideNode`.
* The node type indirection is handled by the record `NodeTypeRef` in
  `internal.node`. You can see that `NodeImpl` has a `node-type` field -
  this is actually a `NodeTypeRef`. `deref`ing a `NodeTypeRef` will
  do a lookup in the node type registry.
* The node type registry is a simple `(def node-type-registry-ref (atom {}))` in `internal.node`
* `g/node.value` is in `dynamo.graph`, which eventually dispatches to
  the `produce-value` method on a `NodeImpl` or `OverrideNode`.
  
g/defnode
---------

### Pseudo code

* Translate the defnode forms to a map representation, via
  `internal.node/process-node-type-forms`. In this representation we
  have lots of generated code as (fn [...] ...) forms
* Find all these function forms and their locations
* Replace the function forms with a generated function name "dollar-name"
* Emit `def`'s implementing the node type

Note that when the bundled editor is started, there is no macro
expansion going on - only the emitted defs remain of the `(g/defnode
...)` forms.

### Emitted defs

We don't only want to be able to reload node definitions. We also want
to be able to reload the definitions of types we mention in the
schemas in the node definition. For this reason, there is actually yet
another "registry" and supporting ref types
(`value-type-registry-ref`, `ValueTypeRef` in `internal.node`).  To
support this we emit a list of calls to `in/register-value-type`, one
for each relevant type mentioned in the schemas.

Next, we emit a list of `def`'s for all the generated functions we
found in the map representation. The name of this `def` obviously
matches the name we used to replace the forms.

Next, in what I guess is an attempt to reduce the size of the finally
generated namespace init class (`gui__init.class` for the gui
namespace for instance) - we emit a "runtime definer" function that
simply returns the map representation of the defnode as a map literal.

Then we emit a call to `internal.node/register-node-type`. This
registers a `map->NodeTypeImpl`'d version of the runtime definer's
return value in the global node type registry. The result of the
registration is a `NodeTypeRef` which we `def` to the node type name,
as given in the `g/defnode`form. This `def` is what we use whenever we
use the node type name in code.

Finally, we have support for a somewhat awkward form inheritance
between node types. As part of that we emit `derive` calls matching
the `inherits` clauses of the node definition.

`internal.node/process-node-type-forms`
---------------------------------------

### Pseudo code

* Add intrinsic outputs and properties that all node types
  share. These are the `_node-id` property, the `_output-jammers`
  property (used when marking a node defective), the `_properties`
  output and the `_overridden-properties` output (used by
  `OverrideNode`). These only need to be added if this is a root node
  type, i.e. doesn't inherit another node type.
* Parse the various clauses (`inherits`, `property`, `output`,
  `input`, `display-order`). This will also collect any types used for
  schemas.
* Implement inheritance. This basically means copy the inherted node
  types' parsed clauses. The main difference is that any generated
  functions in the inherted node type will have been replaced by the
  corresponding `def`d function name.
* Wrap any constant functions. Wherever you can put a production
  function (`g/fnk` or `g/defnk`'d symbol) you can also put a
  constant, like `1`, and this will automatically wrap it in a `(g/fnk
  [] 1)`.
* Figure out the display order of properties. This is controlled by
  the order `property`'s appear, the `display-order` clauses, and any
  `inherit`'ed node types.
* Extract the argument names off supplied production functions, and
  record them as both needed arguments and initial dependencies for
  the function. For "inline" `(g/fnk [...] ...)` forms this is a
  matter of parsing the form. For `(g/defnk [...] ...)`'d symbols,
  like `produce-scene`, we need to resolve the symbol and then check
  it's meta `:arguments`. See `internal.util/inputs-needed` and
  `dynamo.graph/fnk`for details. We
* For each property, we "lift" the dependencies of all `dynamic`'s and
  any `value` clause to the dependencies of the property. ***I haven't
  looked into this properly but beleive it's to make sure the
  `_properties` gets transitively dependent on the dependencies of
  dynamics.***







