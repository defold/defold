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
namespace of the node definition, and `g/node-value` eventually
dispatches to that. Initial implementations of the graph system did
all this at runtime by inspecting the node type information but this
was too inefficient.

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

In addition to reloading node type definitions, we also want to be
able to reload the definitions of types we mention in the schemas in
the node definition. For this reason, there is actually yet another
"registry" and supporting ref types (`value-type-registry-ref`,
`ValueTypeRef` in `internal.node`). Types created with `g/deftype` end
up here and support reloading. But we also support using java types
directly. These are not expected to be redefined, but must still
follow the same interface as the `g/deftype` types. To support this we
emit a list of calls to `in/register-value-type`, one for each
relevant type mentioned in the schemas.

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
  `input`, `display-order`). This steps translates the former node
  type _forms_ into a nested "almost" node type _map_. This will also
  collect any types used for schemas. Note that `property` clauses
  automatically create a corresponding `output` with the same
  name. That `output` can however be overridden.
* Implement inheritance. This basically means copy the inherted node
  types' parsed clauses. The main difference is that any generated
  functions in the inherted node type will already have been replaced
  by the corresponding `def`d function name.
* Wrap any constant functions. Wherever you can put a production
  function (`g/fnk` or `g/defnk`'d symbol) you can also put a
  constant, like `1`, and this step will automatically wrap it in a
  `(g/fnk [] 1)`.
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
  looked into this properly but believe it's to make sure the
  `_properties` gets transitively dependent on the dependencies of
  dynamics.*** We also copy the dependencies of the property to the
  auto generated output with the same name. ***This I believe is too
  pessimistic. Can't see why the output automatically should be
  dependent on the property 'dynamic' dependencies for instance. Also,
  an explicit output with the same name is not necessarily dependent
  on the property at all. See
  `internal.node/apply-property-dependencies-to-outputs`***
* Add a magic output `_declared-properties` (used by the intrinsic
  `_properties` output), that depends on all, well, declared
  properties.
* Collect all the (transitive) dependencies of the output functions
  into a map dependendency -> dependant output. This map
  (`g/input-dependencies`) is a key part of figuring out what (cached)
  outputs might have been invalidated by a change to the graph (see
  `internal.graph/update-graph-successors`).
* Generate function forms for what should happen - called behavior -
  when you do `g/node-value` on an input, property or
  output. Discussed in detail below.
* Collect all inputs marked as `:cascade-delete` - every node
  connected to these will be deleted together with the owning node.
* Collect the names of all non-intrinsic properties, available as
  `g/declared-property-labels`. This is used for instance for
  reflection-like initialisation of nodes "for every declared
  property label set the property to some value from a map".
* Finally, we validate that all arguments mentioned by production
  functions actually refer to an input/property/output of the node.
  
### Behavior of inputs, property and outputs

"Behavior" here means what happens whenever you do `g/node-value`. If
you look at `internal.node/NodeImpl` `produce-value` you see that a
behavior for the label in question is looked up in the nodes
corresponding node type, and then we call the `:fn` of this behavior,
passing the node instance, the label and evaluation context as
arguments. The process is the same regardless if you ask for the value
of an input, property or output. So what needs to happen?

For an input we trace any arc back to find what is connected to the
input, and ask the source node to produce the output. If it's an
`:array` input, we follow all arcs. Then, if a `:substitute` function
is specified and the value (or _any_ value in the case of `:array`) is
an error, the `:substitute` function gets a chance to repair the result
(see `internal.node/node-input-value-function-form`).

For a property, we mentioned that a corresponding output was created
automatically. The production function for this output is essentially
either what was supplied as a `(value ...)` clause. If no such clause
was given, a special case in the behavior of output's will be invoked.
This turns into a `get-property` on the node instance, which in the
case of `NodeImpl` is just a map lookup on the instance itself.

For outputs, both automatically generated from properties and
explicitly provided, there is a whole lot going on (see
`internal.node/node-output-value-function-form`).

* Check if the output har been "jammed". When we do `mark-defective`
  on a node, we populate the intrinsic `_output-jammers` map property
  with all the jammable outputs paired with the supplied error
  value. If the requested output appears in `_output-jammers` the
  corresponding error value is immediately returned.
* Check if this output is generated from a property without `(value
  ...)` clause, and if so, we do a `get-property`.
* Make a note in the evaluation context about what node and output is
  being produced. If this node + output is already "in production" it
  means we have an evaluation cycle, and we assert. ***This is an
  excellent place to instead return an error value with a trace of how
  we got here. Would help solve part of the common cycles error.***.
* Check if the asked value is in any of the caches. Any?  There are in
  fact three caches in effect: The system cache from when the
  evaluation context was created, a local cache of cached outputs
  being produced during this `node-value` call (to be added to the
  system cache), and finally a local temp cache with the result of all
  outputs (regardless if `:cached`) produced during this
  `node-value`. The local temp cache is there to prevent recalculating
  the same value several times.
* Compute the values of the arguments of the production function. This
  can lead to further `produce-value` the argument is another output
  on the same node, or input from another node. Also there are special
  considerations for what argument names mean, this will be described below.
* Check if any of the arguments is an error. In that case, the
  production function will never be called, instead an aggregated
  error will be returned.
* Call the production function (`g/fnk`, `g/defnk`) of the output,
  passing the arguments.
* Schema check the resulting value
* Cache the result in the local cache if `:cached` or the local temp
  cache otherwise.
  
At relevant points, we do checks and callbacks for tracing (for
debugging and/or progress reporting during `node-value`) and also
check if this evaluation is a `:dry-run` - meaning we want to do
essentially everything _except_ calling the actual production
functions.

Keep in mind that all of this is purely syntactical. At this point
we're generating _forms_ implementing the wanted behavior
function. These forms become part of the node type map (under the
`:behavior` key), but there is no actual `fn` to call until the
`g/defnode` macro extracts them and emits a list of `def`'s.

#### Production function arguments

The arguments of a production function (`(value ...)` clause,
`(dynamic ...)` clause or output function) refer to inputs, outputs or
properties in the node definition. The declaration order of these have
no impact.

 Declaring an explicit `output` with the same name as a `property`
(overriding the property) requires special care. Any argument to an
output production function that has the same name as the output, is
assumed to refer to a property - if there is one. Any other production
function will by default refer to the output.

There is also a special case for properties with `(value ...)`
clause. The `value` should produce the current value of the property
(sort of the inverse of the presumed `(set ...)` clause), but in fact,
any `set-property` transaction will also store the value via
`set-property` on the node instance. For this reason, you can in your
`(value [...])` refer to the property by name - and as argument value
you will via `get-property` get this implicitly set value. ***This,
I'm not that happy about***

See `internal.node/fnk-argument-form` for details.

***We pass the name of the current output + requested argument to
`fnk-argument-form`. In the case of property dynamics, we pass the
name of the dynamic as the output - this looks wrong.***

#### Behavior of _declared-properties

For `_declared-properties` there is no production function. Instead we
create a custom behavior that collects the property values and
associated `dynamic`s. There is not much magic going on otherwise, see
`internal.node/declared-properties-function-form`.

#### Twist with property behaviors

In the implementation for the `update-property` transaction, we need
to produce the current/previous value of the property. In this case,
we cannot use `g/node-value` mainly for one reason: Any overriding
output with the same name as the property will effectively hide the
value of the actual property. ***Also, I think we didn't want the
semanting of jamming etc.***

To solve this there is a separate set of behaviors for properties,
called "property behaviors". The property behavior essentially either
does the `get-property` shortcut, or the argument collection, argument
error check and call of the production function (see
`internal.node/node-property-value-function-form`,
`internal.node/collect-property-value-form`).

The only place this is invoked is from the `update-property`
transaction, through `internal.node/node-property-value*`. This
function will look up the behavior in the :property-behavior map and
call that instead.

Examples
--------

Some hints for inspecting what happens during `g/defnode`:
* Insert an inspect function call at various places in the long `->`
  pipe in `internal.node/process-node-type-forms`
* Inspect the differences on `node-type-def` before and after the call
  to `util/update-paths`.
* Do a `macroexpand-1` of very simple node types, or see the examples below

### EmptyNodeType
<pre>
(g/defnode EmptyNodeType)
</pre>

Even this simple node type declaration creates a flood of definitions,
so first let have a look at what
`internal.node/process-node-type-forms` does.

#### Function entry

We receive a fully qualified (namespaced) node type symbol
"some.namespace/EmptyNodeType" and an empty list of forms.

#### After adding intrinsics

Our forms are now:

<pre>
((property _node-id :dynamo.graph/NodeID :unjammable)
 (property _output-jammers :dynamo.graph/KeywordMap :unjammable)
 (output _properties :dynamo.graph/Properties (dynamo.graph/fnk [_declared-properties] _declared-properties))
 (output _overridden-properties :dynamo.graph/KeywordMap (dynamo.graph/fnk [_this _basis] (internal.graph.types/overridden-properties _this _basis))))
</pre>

The properties `_node-id` and `_output-jammers` are `:unjammable`
meaning if we mark this node defective, we still get sane values if we
do `g/node-value` for these properties.

#### After parsing the forms

<pre>
{:register-type-info nil, ; we don't refer to any non-g/deftype types in a schema
 :property                                    ; Map of all properties
 {:_node-id
  {:value-type {:k :dynamo.graph/NodeID},
   :flags #{:unjammable},                     ; :unjammable ends up here. This is the only allowed flag for properties.
   :options {}},                              ; For inputs, there is also an "option" :substitute that takes a function as a value.
                                              ; Both properties and outputs always have an empty `:options` map.
  :_output-jammers
  {:value-type {:k :dynamo.graph/KeywordMap},
   :flags #{:unjammable},
   :options {}}},
 :property-order-decl [],                     ; there are no non-intrinsic properties, so the property order list is empty
 :output ; Map of all outputs
 {:_node-id                                   ; Auto generated output for the _node-id property
  {:value-type {:k :dynamo.graph/NodeID},
   :flags #{:unjammable},                     ; :unjammable also ends up here: this is actually the :unjammable flag that gets respected.
   :options {},
   :fn :internal.node/default-fn,             ; Since this property/output does not have a custom (value ...) clause, there is no need to emit
                                              ; a separate production function. We can just use get-property. :internal.node/default-fn is filtered out
											  ; when extracting functions.
   :default-fn-label :_node-id},              ; Need to know the original label at a later stage. Move along. Nothing to see here.
  :_output-jammers
  {:value-type {:k :dynamo.graph/KeywordMap},
   :flags #{:unjammable},
   :options {},
   :fn :internal.node/default-fn,
   :default-fn-label :_output-jammers},
  :_properties
  {:value-type {:k :dynamo.graph/Properties},
   :flags #{:explicit},                       ; the :explicit flag means this output was not auto generated from a property
   :options {},
   :fn (dynamo.graph/fnk [_declared-properties] _declared-properties)},  ; Supplied (intrinsic) production function forms
  :_overridden-properties
  {:value-type {:k :dynamo.graph/KeywordMap},
   :flags #{:explicit},
   :options {},
   :fn
   (dynamo.graph/fnk
    [_this _basis]
    (internal.graph.types/overridden-properties _this _basis))}}}
</pre>

After this step, nothing really exciting happens until we extract the
used argument names and lift the property dependencies.

#### Arguments extracted, property dependencies lifted

To spare some reading we'll do a diff here. The relevant difference is
that arguments to the `fnk`s have now been pulled out to `:argument`
and `:dependencies` sets next to the `:fn`. We also have a `:key`,
`:name`, `:supertypes`, `:input` (***why???***) and
`:display-order-decl` entry.

</pre>
{:property-display-order (internal.node/merge-display-order nil []),
 :key :internal.defnode-test/EmptyNodeType,
 :property
 {:_node-id
  {...
   :dependencies #{}},
  :_output-jammers
  {...
   :dependencies #{}}},
 ...
 :name "internal.defnode-test/EmptyNodeType",
 :output
 {:_node-id
  {...
   :default-fn-label :_node-id,
   :arguments #{:_node-id},
   :dependencies #{:_node-id}},
  :_output-jammers
  {...
   :default-fn-label :_output-jammers,
   :arguments #{:_output-jammers},
   :dependencies #{:_output-jammers}},
  :_properties
  {...
   :fn (dynamo.graph/fnk [_declared-properties] _declared-properties),
   :arguments #{:_declared-properties},
   :dependencies #{:_declared-properties}},
  :_overridden-properties
  {...
   :fn (dynamo.graph/fnk [_this _basis] (internal.graph.types/overridden-properties _this _basis)),
   :arguments #{:_basis :_this},
   :dependencies #{:_basis :_this}}},
 ...
 :supertypes nil,
 :input nil,
 :display-order-decl nil}
</pre>

#### 

* wrap constant fns händer *efter* merge super types, så om
  t.ex. ersätter output :fn's i definition med nil (borde inte
  behövas? eller?)  så kommer de wrappas som (g/fnk [] nil)
