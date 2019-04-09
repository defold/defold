The g/defnode macro and g/node-value
====================================

Overview
--------

The `g/defnode` macro is used for defining new node types: the inputs,
properties and outputs of corresponding nodes, internal dependencies
and meta data about the node type.

Every node instance in the system has a reference to a certain node
type, and this node type is consulted whenever you do `g/node-value`
on the node, or when for example the output of a node is used as input
to another node we're doing `g/node-value` on. There are also
introspection functions that allow us for example to query a node if
it has a certain output or in what order it's properties should be
displayed.

`g/defnode` (which should maybe be called `g/defnodetype`) is
comlpicated for a number of reasons, but foremost:

* (It has gone through extensive changes and accumulated all sorts of
  quirks, naming inconsistencies and unnecessary indirections)
* We want to be able to reload node definitions and have the changes
  apply immediately to already existing nodes
* We want `g/node-value` to be as efficient as possible

The reloading is handled by an indirection between the node instances
and corresponding node type. The node type of a node instance is not
an assoc'ed immutable value, but rather a reference that when `deref`'d
will look up the actual "current revision" of the node type in a
global registry of node types. `g/defnode` will emit code to update
this registry.

The efficiency of `g/node-value` is acheived by having `g/defnode`
inspect the dependencies (arguments) of all production functions in
the node definition and generate code at compile time (macro expand
time) for argument collection, error checking, schema validation etc
and finally calling the actual production function. Essentially every
production function will become a `def`'d function ("behavior
function") in the namespace of the node definition, and `g/node-value`
eventually dispatches to that. Initial implementations of the graph
system did most of this at runtime by inspecting the node type
information but this was too inefficient.

Where is all of this?
---------------------

So we have a couple of concepts now, where are they in the code?

* `g/defnode` is in `/src/clj/dynamo/graph.clj` (`dynamo.graph`), but the
  actual meat of it - which will be discussed shortly - is in
  `process-node-type-forms` in `/src/clj/internal/node.clj` (`internal.node`)
* The node types are all `NodeTypeImpl` records, definition in `internal.node`
* For node instances there are two record types
    - `NodeImpl` for normal nodes
    - `OverrideNode for override nodes

  Both of these are in `internal.node`. The concept of overrides will not be
  covered here, but we'll touch on some of `OverrideNode`.
* The node type indirection is handled by the record `NodeTypeRef` in
  `internal.node`. You can see that `NodeImpl` has a `node-type` field -
  this is actually a `NodeTypeRef`. `deref`ing a `NodeTypeRef` will
  do a lookup in the node type registry.
* The node type registry is a simple `(def node-type-registry-ref (atom {}))` in `internal.node`
* `g/node.value` is in `dynamo.graph`, which eventually dispatches to
  the `produce-value` method on a `NodeImpl` or `OverrideNode`.
  
The g/defnode macro
-------------------

### Pseudo code

* Translate the defnode forms to a map representation, via
  `internal.node/process-node-type-forms`. In this representation we
  have user supplied production functions and generated behavior
  functions as `(fn [...] ...)` forms
* Find all these function forms and their locations
* Replace the function forms with a generated function name "dollar-name"
* Emit "dollar-name" `def`'s for the functions, and setup code
  implementing the node type

Note that when the bundled editor is started, there is no macro
expansion going on - only the emitted `def`'s and setup code remain of
the defnode forms.

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

Next, we emit a list of `def`'s for all the functions we found in the
map representation. The name of these `def` match the name we use to
replace the corresponding forms.

Next, **I think** is an attempt to reduce the size of the finally
generated namespace init class (`gui__init.class` for the gui
namespace for instance) - we emit a "runtime definer" function that
simply returns the node type map as a literal.

Then we emit a call to `internal.node/register-node-type`. This
registers a `map->NodeTypeImpl`'d version of the runtime definer's
return value in the global node type registry. The result of the
registration is a `NodeTypeRef` which we `def` using the node type name,
as given in the `g/defnode` form. This `def` is what we use whenever we
use the node type name in code.

Finally, we have support for a form of inheritance between node
types. As part of that we emit `clojure.core/derive` calls matching
the `inherits` clauses of the node definition.

`internal.node/process-node-type-forms`
---------------------------------------

### Pseudo code

* Add intrinsic outputs and properties that all node types share. You
  can imagine the clauses below inserted at the top of every root node type
  (those with no `inherits` clause).  The `_output-jammers` property
  is used when marking a node defective. The `_overridden-properties`
  output is used for `OverrideNode`s.
* Parse the various clauses (`inherits`, `property`, `output`,
  `input`, `display-order`). This step translates the former node type
  _forms_ into a nested node type _map_, which will be further
  refined. We will also collect any non-`deftype` types used in
  schemas. Note that `property` clauses automatically create a
  corresponding `output` with the same name. That `output` can however
  be overridden.
* Implement inheritance. This roughly means: copy the inherted node
  types' parsed clauses. The main difference is that any production-
  and behavior functions in the inherted node type will already have
  been replaced by their corresponding `def`'d function name.
* Wrap any constant functions. Wherever you can put a production
  function (`g/fnk` or `g/defnk`'d symbol) you can also put a
  constant, like `1`, and this step will automatically wrap it in a
  `(g/fnk [] 1)`. **So we don't need g/constantly**
* Figure out the display order of properties. This is controlled by
  the order `property`'s appear, the `display-order` clauses, and any
  inherited node types.
* Extract the argument names of supplied production functions, and
  record them as both needed arguments and initial dependencies for
  the function. For "inline" `(g/fnk [...] ...)` forms this is a
  matter of parsing the form. For `(g/defnk [...] ...)`'d symbols,
  like `produce-scene`, we need to resolve the symbol and then check
  it's meta `:arguments`. See `internal.util/inputs-needed` and
  `dynamo.graph/fnk` for details.
* For each property, we "lift" the dependencies of all `dynamic`'s and
  any `value` clause to the dependencies of the property. **I haven't
  looked into this properly but believe it's to make sure the
  `_properties` gets transitively dependent on the dependencies of
  dynamics** **We also copy the dependencies of the property to the
  auto generated output with the same name. This I believe is too
  pessimistic. Can't see why the output automatically should be
  dependent on the property 'dynamic' dependencies for instance. Also,
  an explicit output with the same name is not necessarily dependent
  on the property at all. See
  `internal.node/apply-property-dependencies-to-outputs`**
* Add a magic `:cached` output `_declared-properties` (used by the
  intrinsic `_properties` output), that depends on all declared
  properties.
* Collect all the (transitive) dependencies of the output functions
  into a map dependency -> dependant output. This map (available
  through `g/input-dependencies`) is a key part of figuring out what
  (cached) outputs might have been invalidated by a change to the
  graph (see `internal.graph/update-graph-successors`).  **The
  "lifting" of property dependencies to outputs with the same name
  could make this map contain irrelevant entries - we could be
  invalidating outputs unnecessarily**
* Generate function forms for what should happen ("behavior") when you
  do `g/node-value` on an input, property or output. Discussed in
  detail below.
* Collect all inputs marked as `:cascade-delete` - every node
  connected to these will be deleted together with the owning node.
* Collect the names of all non-intrinsic properties, available as
  `g/declared-property-labels`. This is used for instance for
  reflection-like initialisation of nodes "for every declared
  property label set the property to some value from a map".
* Finally, we validate that all arguments mentioned by production
  functions actually refer to an input/property/output of the node.

Intrinsic outputs and properties:


    (property _node-id g/NodeID :unjammable)
    (property _output-jammers g/KeywordMap :unjammable)
    (output _properties g/Properties (g/fnk [_declared-properties]  declared-properties))
    (output _overridden-properties g/KeywordMap (g/fnk [_this _basis] (gt/overridden-properties _this _basis)))

  
### Behavior of inputs, property and outputs

"Behavior" roughly means what happens whenever you do
`g/node-value`. If you look at `internal.node/NodeImpl`
`produce-value` you see that a behavior for the `label` is looked up
in the node's corresponding node type, and then we call the `:fn` of
this behavior, passing the node instance, the label and evaluation
context as arguments. The process is the same regardless if you ask
for the value of an input, property or output. So what needs to
happen?

For an input we trace arcs backwards to find what is connected to the
input, and ask the source node to produce the output. If it's an
`:array` input, we do this for all connected arcs. If a `:substitute`
function is specified, and the produced value (or _any_ value in the
case of `:array`) is an error, the `:substitute` function gets a
chance to repair the result (see
`internal.node/node-input-value-function-form`). **Actually, this
substitution is still TODO**

For a property, we mentioned that a corresponding output was created
automatically. The production function for this output is essentially
what was supplied as a `(value ...)` clause. If no such clause was
given, a special case in the generation of output behavior will kick
in. This turns into a `get-property` on the node instance, which in
the case of `NodeImpl` is just a map lookup on the instance itself.

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
  means we have an evaluation cycle, and we assert. **This is an
  excellent place to instead return an error value with a trace of how
  we got here. Would help solve part of the frequent cycles error**.
* Check if the asked value is in any of the caches. There are three
  caches in effect: The system cache from when the evaluation context
  was created, a local cache of `:cached` outputs being produced
  during this `node-value` call (to be added to the system cache), and
  finally a local temp cache with the result of all
  non-`:cached` outputs produced during this `node-value`. The local
  temp cache is there to prevent recalculating the same value several
  times.
* Compute the values of the arguments of the production function. This
  can lead to further `produce-value` calls if the argument is another
  output on the same node, or an input from an output on another
  node. Also there are special considerations for what argument names
  mean, this will be described below.
* Check if any of the arguments is an error. In that case, the
  production function will never be called, instead the result will be
  an aggregated error.
* Call the production function (`g/fnk`, `g/defnk`) of the output,
  passing the arguments.
* Schema check the resulting value
* Cache the result in the local cache if `:cached`, or the local temp
  cache otherwise.
  
At relevant points, we do callbacks for tracing (for debugging and/or
progress reporting during `node-value`) and also check if this
evaluation is a `:dry-run` - meaning we want to do essentially
everything _except_ calling the actual production functions.

Keep in mind that all of this is purely syntactical. At this point
we're generating _forms_ implementing the wanted behavior
function. These forms become part of the node type map (under the
`:behavior` key), but there is no actual `fn` to call until the
`g/defnode` macro extracts them and emits a list of `def`'s.

#### Production function arguments

The arguments of a production function (proprty `(value ...)` clause,
`(dynamic ...)` clause or explicit output function) refer to inputs,
outputs or properties in the node definition. The declaration order of
these have no impact.

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
`(value ...)` argument list refer to the same property by name - and
as value you will via `get-property` get this implicitly set value. So
this will not lead to infinite recursion. **This feels a bit sketchy,
the value from `(value ...)` and this magic property can easily get
out of sync during resource "refactoring" for instance, where the
`value` clause essentially returns the value of an `input`**

See `internal.node/fnk-argument-form` for details.

**We pass the name of the current output + requested argument to
`fnk-argument-form`. In the case of property dynamics, we pass the
name of the dynamic as the output. This could go wrong if there is
another output with the same name**

#### Behavior of `_declared-properties`

For `_declared-properties` there is no production function. Instead we
create a custom behavior that collects the property values and
associated `dynamic`s. There is not much magic going on otherwise, see
`internal.node/declared-properties-function-form`.

#### Twist with property behaviors

In the implementation for the `update-property` transaction, we need
to produce the current value of the property. In this case, we cannot
use `g/node-value` mainly for one reason: Any overriding output with
the same name as the property will effectively hide the value of the
actual property. **Also, I think we didn't want the semantics of
jamming**

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

Some tips for inspecting what happens during `g/defnode` and dig
further into this:

* Inspect the differences on `node-type-def` before and after the call
  to `util/update-paths`.
* Insert an inspect function call at various places in the long `->`
  pipe in `internal.node/process-node-type-forms`
* Do a `macroexpand-1` of very simple node types, or see the examples below

### EmptyNodeType

Let's see what happens with an empty node type.

    (g/defnode EmptyNodeType)

Even this simple node type declaration creates a flood of
definitions. `g/defnode` quickly dispatches to
`internal.node/process-node-type-forms` so first let have a look at
what happens there.

#### Function entry

We receive a fully qualified (namespaced) node type symbol
"internal.defnode-test/EmptyNodeType" and an empty list of forms.

#### After adding intrinsics

Our forms are now:

    ((property _node-id :dynamo.graph/NodeID :unjammable)
     (property _output-jammers :dynamo.graph/KeywordMap :unjammable)
     (output _properties :dynamo.graph/Properties (dynamo.graph/fnk [_declared-properties] _declared-properties))
     (output _overridden-properties :dynamo.graph/KeywordMap (dynamo.graph/fnk [_this _basis] (internal.graph.types/overridden-properties _this _basis))))

The properties `_node-id` and `_output-jammers` are `:unjammable`
meaning if we mark this node defective, we still get sane values if we
do `g/node-value` for these properties.

#### After parsing the forms

    {:register-type-info nil,                     ; we don't refer to any non-g/deftype types in a schema
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
     :output                                      ; Map of all outputs
     {:_node-id                                   ; Auto generated output for the _node-id property
      {:value-type {:k :dynamo.graph/NodeID},
       :flags #{:unjammable},                     ; :unjammable also ends up here: this is actually the :unjammable flag that gets respected.
       :options {},
       :fn :internal.node/default-fn,             ; Since this property/output does not have a custom (value ...) clause, there is no need to emit
                                                  ; a separate production function. We can just use get-property. :internal.node/default-fn is filtered out
                                                  ; when extracting functions.
       :default-fn-label :_node-id},              ; Implementation detail. Need to know the original label when later on extracting function arguments.
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
       :fn (dynamo.graph/fnk [_this _basis] (internal.graph.types/overridden-properties _this _basis))}}}

After this step, nothing really exciting happens until we extract the
used argument names and lift the property dependencies.

#### Arguments extracted, property dependencies lifted

The relevant difference is that arguments to the `fnk`s have now been
pulled out to `:argument` and `:dependencies` sets next to the
`:fn`. We also have `:key`, `:name`, `:supertypes`, `:input`
(**why?**) and `:display-order-decl` entries.

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
       :arguments #{:_node-id},                   ; <- 
       :dependencies #{:_node-id}},
      :_output-jammers
      {...
       :default-fn-label :_output-jammers,
       :arguments #{:_output-jammers},            ; <-
       :dependencies #{:_output-jammers}},
      :_properties
      {...
       :fn (dynamo.graph/fnk [_declared-properties] _declared-properties),
       :arguments #{:_declared-properties},       ; <-
       :dependencies #{:_declared-properties}},
      :_overridden-properties
      {...
       :fn (dynamo.graph/fnk [_this _basis] (internal.graph.types/overridden-properties _this _basis)),
       :arguments #{:_basis :_this},              ; <-
       :dependencies #{:_basis :_this}}},
     ...
     :supertypes nil,
     :input nil,
     :display-order-decl nil}

#### Added `_declared-properties`, collected transitive dependencies

    {...
     :input-dependencies
     {:_node-id #{:_node-id},
      :_output-jammers #{:_output-jammers},
      :_declared-properties #{:_properties},
      :_basis #{:_overridden-properties}},
     ...
     :output
     {...
      :_declared-properties
      {:value-type {:k :dynamo.graph/Properties},
       :flags #{:cached},
       :arguments #{},
       :dependencies #{}}}                        ; note there is no user supplied :fn here, behavior will be added later
    }

#### Added generated behaviors

The code looks a bit wonky with unnecessary `do`'s etc because of how
the different steps that need to be performed are broken up into
different helper functions.

Explanation in code comments.

    {...
     :behavior
     {:_overridden-properties
      {:fn
       (fn [node label evaluation-context]
         (let [node-id (internal.graph.types/node-id node)]
           ;; Check if the output is jammed: if so, return jam value.
           (or (internal.node/output-jammer node node-id label (:basis evaluation-context))
               ;; Add the current output to the set of "in production" outputs in order to find/prevent circular dependencies
               (let [evaluation-context (internal.node/mark-in-production node-id label evaluation-context)]
                 ;; This (intrinsic) output is not :cached. We still if it has already been produced during this node-value
                 ;; by looking in the temp cache
                 (if-some [result (internal.node/check-local-temp-cache node-id label evaluation-context)]
                   ;; If it was found, we trace that it was found in the :cache
                   (internal.node/trace-expr node-id label evaluation-context :cache
                                             (fn [] (if (= result :internal.node/cached-nil) nil result)))
                   ;; Otherwise, we trace that an output is being produced
                   (internal.node/trace-expr node-id label evaluation-context :output
                                             (fn []
                                               ;; Collect the arguments. Looking at the _overridden-properties definition in node-intrinsics,
                                               ;; it takes _this (the node) and _basis. All production functions are always passed _node-id and _basis.
                                               (let [arguments {:_basis (:basis evaluation-context),
                                                                :_this node,
                                                                :_node-id node-id}]
                                                 ;; The result is either
                                                 ;; * an aggregated error from the arguments - not likely here :)
                                                 ;; * nil, if this was a :dry-run (this skips calling the actual production functions)
                                                 ;; * the result of calling the dollar-name'd definition of the _overridden-properties
                                                 ;;   production function, passing the argument map
                                                 (let [result (or (internal.node/argument-error-aggregate node-id label arguments)
                                                                  (when-not (:dry-run evaluation-context)
                                                                    (#'internal.defnode-test/EmptyNodeType$output$_overridden-properties
                                                                      arguments)))]
                                                   ;; Check that the result is either an error value, or conforms to the schema (KeywordMap)
                                                   (do (internal.node/schema-check-result node-id label evaluation-context
                                                                                          (schema.core/maybe
                                                                                            (schema.core/conditional internal.graph.error-values/error-value?
                                                                                                                     internal.graph.error_values.ErrorValue
                                                                                                                     :else {Keyword Any}))
                                                                                          result)
                                                       ;; The output is not :cached, but we add it to the local temp cache
                                                       (do (internal.node/update-local-temp-cache! node-id label evaluation-context result)
                                                           result)))))))))))},
      :_output-jammers
      {:fn
       (fn [node label evaluation-context]
         ;; Since _output-jammers is an unjammable property, we don't do any jam checking
         (let [node-id (internal.graph.types/node-id node)]
           ;; _output-jammers does not have a custom (value ...) clause, and is traced as
           ;; :raw-property **Naming? :default-property?**
           (internal.node/trace-expr node-id label evaluation-context :raw-property
                                     (fn []
                                       ;; Unless this is a :dry-run, call get-property on the node directly. There is no need to
                                       ;; generate a separate production function / dollar-name'd def for every simple property.
                                       (when-not (:dry-run evaluation-context)
                                         (internal.graph.types/get-property node (:basis evaluation-context) label))))))},
      :_properties
      {:fn
       (fn [node label evaluation-context]
         (let [node-id (internal.graph.types/node-id node)]
           ;; Check if jammed
           (or (internal.node/output-jammer node node-id label (:basis evaluation-context))
               ;; Add to "in production" outputs
               (let [evaluation-context (internal.node/mark-in-production node-id label evaluation-context)]
                 ;; not :cached, but check temp cache
                 (if-some [result (internal.node/check-local-temp-cache node-id label evaluation-context)]
                   (internal.node/trace-expr node-id label evaluation-context :cache
                                             (fn [] (if (= result :internal.node/cached-nil) nil result)))
                   (internal.node/trace-expr node-id label evaluation-context :output
                                             (fn []
                                               ;; Collect arguments. Looking at the production function, _declared-properties is an argument.
                                               ;; Since this is another output, we effectively recurse and produce it.
                                               (let [arguments {:_declared-properties (internal.graph.types/produce-value node :_declared-properties evaluation-context),
                                                                :_node-id node-id,
                                                                :_basis (:basis evaluation-context)}]
                                                 ;; Note! We treat the '_properties' output specially and don't do any argument error checking.
                                                 ;; If you provide an overriding definition of _properties and refer to something that produces
                                                 ;; an error, the corresponding argument will be an error value. This is different from all other
                                                 ;; production functions. Not sure why we do this, or even if we should.
                                                 (let [result (when-not (:dry-run evaluation-context)
                                                                ;; Call the dollar-name'd def of the _properties production function.
                                                                (#'internal.defnode-test/EmptyNodeType$output$_properties arguments))]
                                                   (do
                                                     ;; Schema check result
                                                     (internal.node/schema-check-result node-id label evaluation-context
                                                                                        (schema.core/maybe
                                                                                          (schema.core/conditional
                                                                                            internal.graph.error-values/error-value?
                                                                                            internal.graph.error_values.ErrorValue
                                                                                            :else
                                                                                            {:properties
                                                                                             {Keyword {:node-id Int,
                                                                                                       {:k :validation-problems} Any,
                                                                                                       :value Any,
                                                                                                       :type Any,
                                                                                                       Keyword Any}},
                                                                                             {:k :node-id} Int,
                                                                                             {:k :display-order}
                                                                                             [(conditional
                                                                                                vector?--5399                   ; <- Why this strange form?
                                                                                                [(one Str "category") Keyword]
                                                                                                keyword?
                                                                                                Keyword)]}))
                                                                                        result)
                                                     (do
                                                       ;; Not :cached, but add to local temp cache
                                                       (internal.node/update-local-temp-cache! node-id label evaluation-context result)
                                                       result)))))))))))},
      :_node-id
      {:fn
       (fn [node label evaluation-context]
         (let [node-id (internal.graph.types/node-id node)]
           ;; This implementation is slightly embarrassing since we already have the node-id here.
           ;; Instead it's being treated as any other :unjammable property with no (value ...) clause.
           (internal.node/trace-expr node-id label evaluation-context :raw-property
                                     (fn []
                                       (when-not (:dry-run evaluation-context)
                                         (internal.graph.types/get-property node (:basis evaluation-context) label))))))},
      :_declared-properties
      {:fn
       (fn [node label evaluation-context]
         ;; _declared-properties gets a custom behavior. Since this sample node type does not have any properties, the value-map is empty.
         (let [node-id (internal.graph.types/node-id node)
               display-order (internal.node/property-display-order (internal.graph.types/node-type node (:basis evaluation-context)))
               value-map {}]
           (when-not (:dry-run evaluation-context)
             {:properties value-map,
              :display-order display-order,
              :node-id node-id})))}},
     ...
     :property-behavior                                ; Here are the custom behaviors used for update-property.
     {:_output-jammers
     {:fn
      (fn [node label evaluation-context]
        (let [node-id (internal.graph.types/node-id node)]
          (internal.node/trace-expr node-id :_output-jammers evaluation-context :raw-property
                                    (fn []
                                      (when-not (:dry-run evaluation-context)
                                        (internal.graph.types/get-property node (:basis evaluation-context) :_output-jammers))))))},
     :_node-id
     {:fn
      (fn [node label evaluation-context]
        (let [node-id (internal.graph.types/node-id node)]
          (internal.node/trace-expr node-id :_node-id evaluation-context :raw-property
                                    (fn []
                                      (when-not (:dry-run evaluation-context)
                                        (internal.graph.types/get-property node (:basis evaluation-context) :_node-id))))))}}
     ...}

#### Back to `g/defnode`

After `internal.node/process-node-type-forms` has massaged the
`defnode ...` clauses into a map, `defnode` needs to turn it into a
list of `def`'s and setup calls. We'll go through this below.

    fn-paths (in/extract-def-fns node-type-def)

This will find all `{:fn ...}` maps under the `[:output <label>]`,
`[:property <label> :dynamics]`, `[:property <label> :value]`,
`[:property <label> :default]`, `[:behavior <label>]` and
`[:property-behavior <label>]`keys. `fn-paths` is a list of pairs with
the path to the `{:fn ...}` and the `fn` forms from the map.

    fn-defs (for [[path func] fn-paths]
              (list `def (in/dollar-name symb path) func))

Turn the `fn-paths` into a list of `fn` `def`'s. For instance, the
behavior clauses for `_node-id` will turn into:

    (def EmptyNodeType$behavior$_node-id
      (fn [node label evaluation-context]
       (let [node-id (internal.graph.types/node-id node)]
        (internal.node/trace-expr node-id label evaluation-context :raw-property
                                  (fn []
                                    (when-not (:dry-run evaluation-context)
                                      (internal.graph.types/get-property node (:basis evaluation-context) label)))))))

Next,

    node-type-def (util/update-paths node-type-def fn-paths
                                     (fn [path func curr]
                                       (assoc curr :fn (list `var (in/dollar-name symb path)))))

Here, we update the `{:fn ...}`'s, replacing the function clauses with
a reference to the `def`'d version. Now the behavior for `_node-id` in the map is simply:


    {:fn #'EmptyNodeType$behavior$_node-id}

Next,

    node-key (:key node-type-def)
    derivations (for [tref (:supertypes node-type-def)]
                  `(when-not (contains? (descendants ~(:key (deref tref))) ~node-key)
                     (derive ~node-key ~(:key (deref tref)))))
    node-type-def (update node-type-def :supertypes #(list `quote %))

This translates the `inherits` clauses to `derive`'s so `isa?` etc
works as expected.

    runtime-definer (symbol (str symb "*"))

This is just a name to be use for the function that returns the final
form of the node type map, the "runtime definer". In this case
"EmptyNodeType*".

    type-regs (for [[key-form value-type-form] (:register-type-info node-type-def)]
               `(in/register-value-type ~key-form ~value-type-form))
    node-type-def (dissoc node-type-def :register-type-info)]

This generates calls to register the non-`deftype`'d types we've used,
using `register-value-type`.  We have no other use for this
information so `dissoc` it.

    `(do
       ~@type-regs
       ~@fn-defs
       (defn ~runtime-definer [] ~node-type-def)
       (def ~symb (in/register-node-type ~node-key (in/map->NodeTypeImpl (~runtime-definer))))
       ~@derivations))

Finally, we emit all these `def`'s and function calls.

### ProductionDefs

Here we'll have a look at the different ways of specifying a
production function.

    (g/defnk produce-defnk-output [] "defnk output")
    
    (g/defnode ProductionDefs
      (output fnk-output g/Str (g/fnk [] "fnk output"))
      (output defnk-output g/Str produce-defnk-output)
      (output constant-output g/Str "constant output"))

After `process-node-type-forms`, relevant parts of the node type map looks like this:

    {...
     :output
     {:fnk-output
      {:value-type {:k :dynamo.graph/Str},
       :flags #{:explicit},
       :options {},
       :fn (g/fnk [] "fnk output"),
       :arguments #{},
       :dependencies #{}},
      :defnk-output
      {:value-type {:k :dynamo.graph/Str},
       :flags #{:explicit},
       :options {},
       :fn produce-defnk-output,
       :arguments #{},
       :dependencies #{}},
      :constant-output
      {:value-type {:k :dynamo.graph/Str},
       :flags #{:explicit},
       :options {},
       :fn (dynamo.graph/fnk [] "constant output"),
       :arguments #{},
       :dependencies #{}},
      ...}
     :behavior
     {
     :fnk-output
      {:fn
        (fn [node label evaluation-context]
        ...
          (when-not (:dry-run evaluation-context)
            (#'internal.defnode-test/ProductionDefs$output$fnk-output arguments))
        ...)
      }
     :defnk-output
     {:fn
       (fn [node label evaluation-context]
       ...
         (when-not (:dry-run evaluation-context)
           (#'internal.defnode-test/ProductionDefs$output$defnk-output arguments)))]
       ...)
     }
     :constant-output
     {:fn
       (fn [node label evaluation-context]
       ...
         (when-not (:dry-run evaluation-context)
           (#'internal.defnode-test/ProductionDefs$output$constant-output arguments))
       ...)
     }
     ...
    }

In the `:output` section we see that the `produce-defnk-output`
remains, and the `"constant output"` string has been wrapped in a
`fnk`. In the behavior section, we see that all behavior functions
contain a call to a dollar-name'd production function.

Checking what `g/defnode` emits we see:

    (def ProductionDefs$output$fnk-output (g/fnk [] "fnk output"))
    (def ProductionDefs$output$defnk-output produce-defnk-output)
    (def ProductionDefs$output$constant-output (dynamo.graph/fnk [] "constant output"))

### CustomProperty

Here we demonstrate

* Property `(value ...)` clauses referring to itself
* A look at `:property-behavior` and how it differs from the usual `:behavior`
* Where `dynamic`s and the `default` value function end up
* How the properties end up in `_properties`, and property dynamics

    (g/defnode CustomProperty
      (property simple-property g/Str)
      (property custom-property g/Str
                (default (fn [] "fruit"))
                (value (g/fnk [simple-property custom-property]
                         (str simple-property custom-property)))
                (dynamic matches-input (g/fnk [custom-property simple-input]
                                         (= custom-property simple-input))))
      (input simple-input g/Str))

#### Property `(value ...)` clauses referring to itself

The `(value ...)` clause for `:custom-property` ends up as an
production function in the `:output` section:

    :custom-property
    {:value-type {:k :dynamo.graph/Str},
     :flags #{},
     :options {},
     :fn (g/fnk [simple-property custom-property] (str simple-property custom-property)),
     :arguments #{:simple-property :custom-property},
     :dependencies #{:simple-property :custom-property}}

And the corresponding behavior in the `:behavior` section:

    (fn [node label evaluation-context]
      (let [node-id (internal.graph.types/node-id node)]
        (or (internal.node/output-jammer node node-id label (:basis evaluation-context))
        ...
        ;; Here we see that the "self reference" to custom-property became a direct property access via get-property,
        ;; instead of a (failing) recursive call to produce-value for the same label.
        (let [arguments {:simple-property (internal.graph.types/produce-value node :simple-property evaluation-context),
                         :custom-property (internal.node/trace-expr node-id :custom-property evaluation-context :raw-property
                                                                    (fn []
                                                                      (when-not (:dry-run evaluation-context)
                                                                        (internal.graph.types/get-property node (:basis evaluation-context) :custom-property)))),
                         :_node-id node-id,
                         :_basis (:basis evaluation-context)}]
          (let [result (or (internal.node/argument-error-aggregate node-id label arguments)
                           (when-not (:dry-run evaluation-context)
                             (#'internal.defnode-test/CustomProperty$output$custom-property arguments)))]
                             ...

#### A look at `:property-behavior` and how it differs from the usual `:behavior`

The `:property-behavior` for `custom-property` (used during `update-property`), looks like this:

    (fn [node label evaluation-context]
      (let [node-id (internal.graph.types/node-id node)]
        ;; No jam checking, no check for cycles
        (internal.node/trace-expr node-id :custom-property evaluation-context :property
                                  (fn []
                                    ;; The reference to custom-property will not use produce-value, but get-property, to avoid failing recursion.
                                    (let [arguments__11811__auto__ {:simple-property (internal.graph.types/produce-value node :simple-property evaluation-context),
                                                                    :custom-property (internal.graph.types/get-property node (:basis evaluation-context) :custom-property)}]
                                      (or (internal.node/argument-error-aggregate node-id :custom-property arguments__11811__auto__)
                                          ;; **Small wart: if :dry-run, we will call (constantly nil) with the argument.
                                          ;; This was a simple way to compose the code generating functions**
                                          ((if-not (:dry-run evaluation-context)
                                             ;; Note that we call ...$property$custom-property$value instead of ...$output$...
                                             ;; This prevents us from calling the production function of an explicit
                                             ;; overriding output.
                                             #'internal.defnode-test/CustomProperty$property$custom-property$value (constantly nil)) arguments__11811__auto__)))))))

#### Where `dynamic`s and the `default` value function end up

Property dynamics and the default function end up under the
corresponding property in the `:property` section of the node type
map.

As you can see, the production function for `(value ...)` also ends up
here in the `:property` map. This version will be used for
`_properties` later on.

    :property
    {...
     :custom-property
     {:value-type {:k :dynamo.graph/Str},
      :flags #{},
      :options {},
      :default {:fn (fn* ([] "fruit")), :arguments #{}, :dependencies #{}},
      :value
      {:fn (g/fnk [simple-property custom-property] (str simple-property custom-property)),
       :arguments #{:simple-property :custom-property},
       :dependencies #{:simple-property :custom-property}},
      :dynamics
      {:matches-input
       {:fn (g/fnk [custom-property simple-input] (= custom-property simple-input)),
        :arguments #{:simple-input :custom-property},
        :dependencies #{:simple-input :custom-property}}},
      :dependencies #{:simple-property :simple-input}}}

All these `fn`'s will be emitted as `def`'s by `defnode` and the node
type map updated with the corresponding vars.

The `:default` function will by called when constructing a new node to
provide values if none were supplied.

#### How the properties end up in `_properties`, and property dynamics

The intrinsic version of `_properties` just passes through
`_declared-properties`. Looking at the `_declared-properties` behavior
for this node type:

    (fn [node label evaluation-context]
      (let [node-id (internal.graph.types/node-id node)
            display-order (internal.node/property-display-order (internal.graph.types/node-type node (:basis evaluation-context)))
            value-map {:simple-property {:value (internal.node/trace-expr node-id :simple-property evaluation-context :raw-property
                                                                          (fn []
                                                                            (when-not (:dry-run evaluation-context)
                                                                            ;; The simple-property has no custom (value ...), so we just do get-property
                                                                              (internal.graph.types/get-property node (:basis evaluation-context) :simple-property)))),
                                         :type {:k :dynamo.graph/Str},
                                         :node-id node-id},
                       :custom-property {:value (internal.node/trace-expr node-id :custom-property evaluation-context :property
                                                                          (fn []
                                                                            ;; Below gets generated pretty much the same way as the :property-behavior above
                                                                            (let [arguments__11811__auto__ {:simple-property (internal.graph.types/produce-value node :simple-property evaluation-context),
                                                                                                            :custom-property (internal.graph.types/get-property node (:basis evaluation-context) :custom-property)}]
                                                                              (or (internal.node/argument-error-aggregate node-id :custom-property arguments__11811__auto__)
                                                                                  ((if-not (:dry-run evaluation-context)
                                                                                     #'internal.defnode-test/CustomProperty$property$custom-property$value (constantly nil)) arguments__11811__auto__))))),
                                         :type {:k :dynamo.graph/Str},
                                         ;; Here comes our `dynamic`
                                         :matches-input (internal.node/trace-expr node-id [:custom-property :matches-input] evaluation-context :dynamic
                                                                                  (fn []
                                                                                    ;; Collect arguments for the dynamic. Note that the reference to simple-property will prioritise an output with the same name, since we use produce-value.
                                                                                    (let [arguments__11811__auto__ {:simple-input (internal.node/error-checked-input-value node-id :simple-input (internal.node/pull-first-input-value node :simple-input evaluation-context)),
                                                                                                                    :custom-property (internal.graph.types/produce-value node :custom-property evaluation-context)}]
                                                                                      (or (internal.node/argument-error-aggregate node-id :matches-input arguments__11811__auto__)
                                                                                          ((if-not (:dry-run evaluation-context)
                                                                                             ;; The dynamic gets a slightly longer dollar-name :)
                                                                                             #'internal.defnode-test/CustomProperty$property$custom-property$dynamics$matches-input (constantly nil)) arguments__11811__auto__))))),
                                         :node-id node-id}}]
        (when-not (:dry-run evaluation-context)
          {:properties value-map,
           :display-order display-order,
           :node-id node-id}))

### OverridenProperty

Lets have a look what happens when you have an output with the same
name as a property.

    (g/defnode OverriddenProperty
      (property data g/Str (value (g/fnk [] 1)))
      (input nonsense-input g/Str)
      (output data g/Str (g/fnk [data nonsense-input]
                           (str data nonsense-input))))

In the `:output` section, only the explicit output production function
remains, but the definition in `:property` remains.


    {:output
     {:data
      {:value-type {:k :dynamo.graph/Str},
       :flags #{:explicit},
       :options {},
       :fn (g/fnk [data nonsense-input] (str data nonsense-input)),
       :arguments #{:nonsense-input :data},
       :dependencies #{:nonsense-input :data}}
      ...
     }
     ...
     :property
     {...
      :data
      {:value-type {:k :dynamo.graph/Str},
       :flags #{},
       :options {},
       :value {:fn (g/fnk [] 1), :arguments #{}, :dependencies #{}},
       :dependencies #{}}}
    }

If you look into the code generated for the data output behavior, you
would find that it calls
`#'internal.defnode-test/OverriddenProperty$property$data$value` to get the property value. 
Same thing for the `_declared-properties` output and the
`:property-behavior` for data.

### InputVarieties

We generate behaviors for inputs also, **These are somewhat
broken in that they do not perform error substitution**. They are
used in a handful places in the editor (we do `(g/node-value
... :some-input)`) and are sometimes useful for debugging, but we could
probably do without them.

    (g/defnode InputVarieties
      (input single-input g/Str)
      (input array-input g/Str :array)
      (input subst-single-input g/Str :substitute (fn [err] "error!"))
      (input subst-array-input g/Str :array :substitute (fn [inputs] (map #(if (g/error? %) "error!" %) inputs))))

The behavior (in `:behavior`) looks like this:

    {:array-input {:fn internal.node/pull-input-values},
     :subst-single-input
     {:fn (clojure.core/partial internal.node/pull-first-input-with-substitute (fn [err] "error!"))},
     :subst-array-input
     {:fn (clojure.core/partial internal.node/pull-input-values-with-substitute
                                (fn [inputs]
                                  (map
                                    (fn* [p1__26210#] (if (g/error? p1__26210#) "error!" p1__26210#))
                                    inputs)))},
     :single-input {:fn internal.node/pull-first-input-value}}

The inputs do not need individual production functions - these are the
four varieties possible.

**Sadly both `-with-substitute` varieties say "todo - invoke substitute"**

### InputVarietiesUsed

If, however we use these inputs as argument to a production function:

    (g/defnode InputVarietiesUsed
      (input single-input g/Str)
      (input array-input g/Str :array)
      (input subst-single-input g/Str :substitute (fn [err] "error!"))
      (input subst-array-input g/Str :array :substitute (fn [inputs] (map #(if (g/error? %) "error!" %) inputs)))
      (output output-single-input g/Str (g/fnk [single-input] single-input))
      (output output-array-input g/Str (g/fnk [array-input] array-input))
      (output output-subst-single-input g/Str (g/fnk [subst-single-input] subst-single-input))
      (output output-subst-array-input g/Str (g/fnk [subst-array-input] subst-array-input)))

The behavior for `:output-subst-array-input` for instance will, when
collecting arguments, perform the error substitution:

    (let [arguments {:subst-array-input (internal.node/error-substituted-array-input-value
                                          (internal.node/pull-input-values node :subst-array-input evaluation-context)
                                          (fn [inputs]
                                            (map (fn* [p1__26365#]
                                                      (if (g/error? p1__26365#) "error!" p1__26365#))
                                                 inputs))),
                     :_node-id node-id,
                     :_basis (:basis evaluation-context)}]

### CachedOutputs

How does the `:cached` flag affect the behavior function? The
difference is only what caches we use for lookup and update.

    (g/defnode CachedOutputs
      (property data g/Str)
      (output non-cached g/Str (g/fnk [data] data))
      (output cached g/Str :cached (g/fnk [data] data)))

    {...
     :behavior
     {...
      :cached
      {:fn
       (fn [node label evaluation-context]
         (let [node-id (internal.graph.types/node-id node)]
           (or (internal.node/output-jammer node node-id label (:basis evaluation-context))
               (let [evaluation-context (internal.node/mark-in-production node-id label evaluation-context)]
                 ;; internal.node/check-caches! will look in two caches:
                 ;; * The "global" copied from the system when creating the evaluation context
                 ;; * The "local" which contains the results of newly produced :cached outputs.
                 ;;   These will typically be added to the system cache at the end of the `node-value` call.
                 (if-some [[result] (internal.node/check-caches! node-id label evaluation-context)]
                   result (internal.node/trace-expr node-id label evaluation-context :output
                                                    (fn []
                                                      (let [arguments {:data (internal.graph.types/produce-value node :data evaluation-context),
                                                                       :_node-id node-id,
                                                                       :_basis (:basis evaluation-context)}]
                                                        (let [result (or (internal.node/argument-error-aggregate node-id label arguments)
                                                                         (when-not (:dry-run evaluation-context)
                                                                           (#'internal.defnode-test/CachedOutputs$output$cached
                                                                             arguments)))]
                                                          (do (internal.node/schema-check-result node-id label evaluation-context
                                                                                                 (schema.core/maybe
                                                                                                   (schema.core/conditional
                                                                                                     internal.graph.error-values/error-value?
                                                                                                     internal.graph.error_values.ErrorValue
                                                                                                     :else
                                                                                                     java.lang.String))
                                                                                                 result)
                                                              ;; Here internal.node/update-local-cache! will, well, add the result to the local cache
                                                              (do (internal.node/update-local-cache! node-id label evaluation-context result)
                                                                  result)))))))))))},
      :non-cached
      {:fn
       (fn [node label evaluation-context]
         (let [node-id (internal.graph.types/node-id node)]
           (or (internal.node/output-jammer node node-id label (:basis evaluation-context))
               (let [evaluation-context (internal.node/mark-in-production node-id label evaluation-context)]
                 ;; Non-:cached outputs will, as we've already seen, only use the local temp cache
                 (if-some [result (internal.node/check-local-temp-cache node-id label evaluation-context)]
                   (internal.node/trace-expr node-id label evaluation-context :cache
                                             (fn []
                                               (if (= result :internal.node/cached-nil)
                                                 nil
                                                 result)))
                   (internal.node/trace-expr node-id label evaluation-context :output
                                             (fn []
                                               (let [arguments {:data (internal.graph.types/produce-value node :data evaluation-context),
                                                                :_node-id node-id,
                                                                :_basis (:basis evaluation-context)}]
                                                 (let [result (or (internal.node/argument-error-aggregate node-id label arguments)
                                                                  (when-not (:dry-run evaluation-context)
                                                                    (#'internal.defnode-test/CachedOutputs$output$non-cached
                                                                      arguments)))]
                                                   (do (internal.node/schema-check-result node-id label evaluation-context
                                                                                          (schema.core/maybe
                                                                                            (schema.core/conditional
                                                                                              internal.graph.error-values/error-value?
                                                                                              internal.graph.error_values.ErrorValue
                                                                                              :else
                                                                                              java.lang.String))
                                                                                          result)
                                                       ;; And here, we only update the local temp cache
                                                       (do (internal.node/update-local-temp-cache! node-id label evaluation-context result)
                                                           result)))))))))))}
     }
    }

### BaseNode, DerivedNode

When using `inherits`, be careful not to unintentionally override a
property from the base node type. The semantics of `inherits` is
almost "merge the node type map of the base node type with the
current" / copy-paste the definition.

    (g/defnode BaseNode
      (property surprise g/Str
                (dynamic dynamic-value (g/fnk [surprise] surprise)))
      (output use-surprise g/Str (g/fnk [surprise] surprise)))
    
    (g/defnode DerivedNode
      (inherits BaseNode)
      (output surprise g/Str (g/fnk [] "DerivedNode/surprise")))

With this definition, the behavior for `use-surprise` will call
`produce-value` to get the argument for `surprise`. So on a
`DerivedNode`, `use-surprise` will actually return
"DerivedNode/surprise". The `_declared-properties` behavior will
correctly call the production function for the property `surprise` to
get the value, but `dynamic-value` will use `produce-value` and get
"DerivedNode/surprise". This might or might not be surprising.

For reference, the `derive` calls emitted by `defnode` are:

    (when-not (contains? (descendants :internal.defnode-test/BaseNode) :internal.defnode-test/DerivedNode)
      (derive :internal.defnode-test/DerivedNode :internal.defnode-test/BaseNode))
