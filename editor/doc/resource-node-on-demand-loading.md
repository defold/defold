# On-demand loading of `ResourceNode` subgraphs

Status: design exploration

## Summary

The editor is already close to being able to represent an unloaded resource:
project loading creates a concrete `ResourceNode` for every workspace resource,
and the existing `.defunload` support can leave that node in the graph with its
`:loaded` property set to `false`. What is missing is a safe link between output
evaluation and materialization of the node's source data and owned subgraph.

The recommended direction is:

1. Keep a stable, lightweight `ResourceNode` shell for every project resource.
2. Introduce a project-level materialization coordinator that reuses the
   existing read, dependency-ordering, load, save-state, and defective-node
   machinery.
3. Add a graph evaluation barrier for materializing outputs. The barrier must
   signal that loading is required; it must not transact from inside an output
   producer.
4. Handle the signal at a load-aware evaluation boundary. Load the resource and
   its structural dependency closure in a non-undoable transaction, create a new
   evaluation context, then restart evaluation.
5. Initially retain a small eager set for project-wide services and resource
   types whose load functions have global effects. Reduce that set as those
   systems are made compatible with partial loading.

This is more work than calling a load function from `g/node-value`, but it
preserves the graph's snapshot and cache semantics and gives us a path to
incremental adoption.

## Goals

- Avoid reading and materializing most resources during project open.
- Preserve the stable project resource lookup from proj-path to node-id.
- Produce the same values and graph structure after materialization as the
  eager loader does today.
- Preserve dependency load order, overrides, save/dirty behavior, resource
  sync, error reporting, and deterministic loading.
- Load each resource at most once per node incarnation, even under concurrent
  evaluation.
- Make unintended eager fan-out measurable and testable.

The first iteration should not attempt to unload an already-materialized node.
Eviction is substantially harder because views, selections, overrides, undo,
and external callers may retain ids of owned nodes.

## Terminology

- **Resource shell**: the concrete resource-type node with `:resource`,
  `:_node-id`, and materialization state, but without source values, load-fn
  connections, or owned substructure.
- **Substructure**: nodes created by a resource's `:load-fn` and owned through
  `:cascade-delete` connections, including override structures created from
  referenced resources.
- **Materialization**: reading the resource, storing disk/source state, invoking
  its `:load-fn`, and applying the resulting transaction data.
- **Structural dependency**: a referenced resource that must be materialized
  before the referencing resource's `:load-fn` runs. Overrides of collections,
  game objects, and GUI scenes are the main reason load order matters.

## Current behavior

### Initial project load

[`editor.defold-project/load-project!`](../src/clj/editor/defold_project.clj)
currently does the following:

1. `make-node-id+resource-pairs` reserves deterministic node-ids for all
   workspace resources.
2. `make-resource-nodes-tx-data` creates every concrete `ResourceNode` and
   connects its `:node-id+resource` output to the `Project` node.
3. `read-nodes` reads source values and uses each resource type's
   `:dependencies-fn` to discover resource references.
4. `sort-node-load-infos-for-loading` orders dependencies before consumers.
5. `load-nodes!` stores source and disk state, sets `:loaded` to `true`, invokes
   the resource `:load-fn`, and commits all generated substructure and
   connections.
6. Save values are primed in the system cache.

The `game.project` resource type uses its `:connect-fn` to connect the resource
shell to project-wide settings before resource load-fns run. This is an example
of a bootstrap dependency that must remain explicit.

### Existing partial-loading mechanisms

There are two related mechanisms, but neither implements load-on-output:

- Resource type `:lazy-loaded` currently means that selected code/text resource
  types do not retain their full source value at project load. Their load-fn is
  still invoked and their `ResourceNode` is marked loaded. The name should be
  changed before introducing general node materialization; something like
  `:lazy-source-value?` would describe its current meaning.
- `.defunload` can leave a concrete resource node in the graph with
  `resource-node/loaded?` false. `connect-resource-node` can later materialize an
  unsafe referenced resource inside the transaction that creates the reference.
  Resource types marked `:allow-unloaded-use` may deliberately produce limited
  outputs without materializing.

There are also two different notions of "loaded":

- `resource/loaded?` is a property of `FileResource` derived from `.defunload`.
  It describes user policy/eligibility.
- `resource-node/loaded?` reads the graph node's hidden `:loaded` property. It
  describes whether its load-fn has run.

The design should name these concepts separately, for example **eligible** and
**materialized**, rather than adding more meaning to `loaded?`.

### Output evaluation does not currently load a shell

The `:loaded` property in
[`editor.resource-node/ResourceNode`](../src/clj/editor/resource_node.clj) is not
a dependency of its other outputs. `g/node-value` dispatches directly to the
generated node behavior, which may return defaults, nil, an empty collection,
or an error from an unloaded shell. It never invokes the resource load-fn.

This behavior is intentional for `.defunload`, but is not sufficient for
transparent automatic deferral.

### Reload and save already understand some unloaded state

Resource sync records whether replaced nodes were loaded and avoids loading the
replacement for a still-unloaded `.defunload` resource. Override transfer can
target an unloaded replacement; override nodes for later-created substructure
are expanded when the target is eventually loaded.

The save system only connects a resource to `Project :save-data` from its
load-fn (or equivalent resource-specific setup). This is useful for demand
loading: evaluating "save all" need not materialize every shell, and an
unmaterialized resource cannot be dirty.

## Graph topology findings

### Refined isolation rule and audit method

Complete graph encapsulation is not required for first-load deferral. Inputs
from resource shells or the `Project` node into substructure are expected, and
the implicit connections introduced by override propagation are part of the
override mechanism. Two related checks are needed:

> An unmaterialized resource has no substructure. Any explicit connection from
> one substructure node to another must remain within the same ultimate
> `ResourceNode` owner. A substructure node may feed its owning shell.

A **global-effect edge** is an explicit connection from a resource shell or its
owned substructure into an ownerless/global node. Examples of global nodes are
`Project`, script intelligence, editor extensions, and project-wide
localization/annotation services. These edges are not necessarily ownership
violations. However, if they are installed by a load-fn, global state depends on
which load-fns have run. Installing shell-originating registrations with a
`:connect-fn` instead makes the complete contributor set available as soon as
the resource shells exist. `:save-data` registrations are temporarily excluded
from this category.

This permits a resource load-fn to wire external shell and project outputs into
newly-created nodes. It also permits the resource's substructure to aggregate
values back into its own shell. Neither case directly couples the lifetimes of
two resources' subgraphs.

The runtime audit used `test/resources/save_data_project`, the broad save-data
fixture containing all editor resource categories. It considered explicit arcs
between non-override nodes and assigned ownership by recursively following
explicit connections into `:cascade-delete` inputs. This intentionally excludes
implicit override-propagation arcs, which are allowed by the rule. It also
avoids using `owner-resource-node-id` as the sole audit primitive: in the
presence of overrides, an original node can acquire implicit ownership paths
that make a first-match traversal misleading.

The loaded fixture contained:

| Item | Count |
| --- | ---: |
| Total nodes in the project graph | 2,295 |
| Non-override nodes considered by the audit | 2,138 |
| Override nodes excluded from the explicit-ownership audit | 157 |
| Resource shells | 1,732 |
| Concrete resource-shell node types | 61 |
| Resource shells with original substructure | 42 |
| Original substructure nodes | 401 |
| Nodes with ambiguous explicit ownership | 0 |

This is a correctness-oriented topology snapshot, not a load-time benchmark.
A secondary scan found 129 explicit arcs touching the 157 override nodes.
Fourteen override nodes had an explicit cascade chain to a resource owner, and
none of their explicit arcs crossed between the classified owners. The other
143 derive ownership only through the permitted implicit override mechanism and
remain outside the base-graph assertion. A production diagnostic should retain
this separate override-provenance category instead of guessing an owner.

### Direct cross-owner isolation holds in the fixture

The explicit arc classification was:

| Arc category | Count | Assessment |
| --- | ---: | --- |
| Substructure to substructure, same owner | 1,439 | Internal |
| Substructure to owning resource shell | 1,189 | Internal aggregation |
| Resource shell to substructure | 728 | Allowed input |
| `Project` to substructure | 20 | Allowed input |
| Substructure to substructure, different owners | 0 | Isolation violation |
| Substructure to foreign resource shell | 0 | Suspicious escape |
| Substructure to ownerless/global node other than `Project` | 0 | Suspicious escape |
| Substructure to `Project` | 3 | Global registration; classified below |

The 20 allowed `Project` inputs cover collision-group data, sprite rendering
defaults, and settings/dependency metadata. The shell inputs are the normal
resource-reference pattern: a material, image, atlas, game object, or other
resource shell supplies data to the consumer's owned nodes after those nodes
have been created.

The result supports deferring original resource substructure without a broad
façade refactor. A production invariant checker should use the same categories
instead of treating every arc that touches substructure as a violation. It
should report override-propagated arcs separately rather than folding them into
explicit cross-owner counts.

### Global registrations

The broader runtime scan starts with explicit arcs whose source is either a
resource shell or its substructure and whose target has no resource owner. It
then excludes the universal `:node-id+resource -> Project
:node-id+resources` shell-registration arcs, removes `:save-data`, and verifies
the remaining connection sites in source. On `save_data_project`, 56 observed
arcs across 17 endpoint patterns connect into global nodes. Fifty-one are now
installed by resource `:connect-fn`s. The five collision-related registrations
remain load-fn-installed and are intentionally deferred for separate treatment:

| Source role and type | Source output | Global target input | Count | Current installer |
| --- | --- | --- | ---: | --- |
| Shell: `LuaNode`, `ScriptNode` | `:breakpoints` | `Project :breakpoints` | 15 | `editor.code.resource/connect-fn` |
| Shell: `LuaNode`, `ScriptNode` | `:required-module-info` | `ScriptIntelligenceNode :required-module-infos` | 11 | `editor.code.script/connect-fn` |
| Shell: `LuaNode` from a dependency archive | `:resource-with-lines` | `ScriptAnnotations :script-annotations` | 8 | `editor.code.script/connect-fn` |
| Shell: `ScriptApiNode` | `:completions` | `ScriptIntelligenceNode :lua-completions` | 5 | `editor.script-api/connect-fn` |
| Shell: file-backed `ScriptApiNode` | `:build-errors` | `ScriptIntelligenceNode :build-errors` | 1 | `editor.script-api/connect-fn` |
| Shell: `CollisionObjectNode` | `:collision-group-node` | `Project :collision-group-nodes` | 2 | `editor.collision-object/load-collision-object` |
| Substructure: `CollisionGroupNode` | `:collision-group-node` | `Project :collision-group-nodes` | 3 | `editor.tile-source/load-tile-source` via `attach-collision-group-node` |
| Shell: `GameProperties` | `:proj-path+meta-info` | `Project :proj-path+meta-info-pairs` | 4 | `editor.game-properties/connect-fn` |
| Shell: `EditorLocalizationNode` | `:resource-path+reader-fn` | `EditorLocalizationBundle :resource-path+reader-fns` | 1 | `editor.editor-localization/connect-fn` |
| Shell: `EditorScript` | `:prototype` | `EditorExtensions :project-prototypes` | 1 | `editor.editor-script` connect-fn |
| Shell: `EditorScript` | `:reload-signature` | `EditorExtensions :project-reload-signatures` | 1 | `editor.editor-script` connect-fn |
| Shell: `GameProjectNode` | `:settings-map` | `Project :settings` | 1 | `editor.game-project/connect-game-project` |
| Shell: `GameProjectNode` | `:display-profiles-data` | `Project :display-profiles` | 1 | `editor.game-project/connect-game-project` |
| Shell: `GameProjectNode` | `:texture-profiles-data` | `Project :texture-profiles` | 1 | `editor.game-project/connect-game-project` |
| Shell: `GameProjectNode` | `:use-font-layout` | `Project :use-font-layout` | 1 | `editor.game-project/connect-game-project` |

The two combined `LuaNode`/`ScriptNode` rows each collapse two
source-type-specific patterns, so 17 runtime patterns appear as 15 table rows.
Of the 56 arcs, 53 originate at resource shells and three at tile-source
substructure. No observed global registration originates at an override node.
The `EditorScript` connect-fn also has conditional `:library-prototypes` and
`:library-reload-signatures` targets for non-file resources; that branch is a
potential global effect but is not exercised by this fixture.

Forty-seven of the connect-time arcs were formerly installed by load-fns; the
other four are the existing `game.project` bootstrap wiring. The game-project
connect-fn also installs an incoming `ScriptIntelligenceNode :build-errors ->
GameProjectNode :build-errors` connection, which is not a global-effect edge
because it points into the resource shell. The same distinction leaves the
incoming Lua preprocessor and completion connections in
`editor.code.script/additional-load-fn`.

As a result, only the five collision-related contributors remain absent before
their resources materialize. A global collision-group consumer can still
observe an incomplete contributor set and cannot discover missing contributors
by ordinary graph traversal.

### Collision groups demonstrate both shell and substructure effects

The collision-group subsystem accounts for five of the 56 global-effect edges:

- three `TileSource`-owned `CollisionGroupNode` substructure nodes connect
  `:collision-group-node` to `Project :collision-group-nodes`; and
- the top-level `CollisionObjectNode` shells for `/checked01.collisionobject`
  and `/checked02.collisionobject` make the same connection.

The tile-source contributors originate in
`/builtins/graphics/particle_blob.tilesource`, `/checked.tilesource`, and
`/checked.tileset`. This fixture has no embedded collision-object component. If
it did, `load-collision-object` would install the same edge from a
`CollisionObjectNode` owned as game-object substructure.

`Project :collision-groups-data` fans the five-contributor aggregate out to
eight targets: three collision-group substructure nodes, three tile-source
shells, and two collision-object shells.

This is an indirect cross-owner dependency even though there is no direct
substructure-to-substructure arc:

```text
resource A substructure
    -> Project aggregate
        -> resource B shell or substructure
```

It creates two issues for on-demand loading:

1. The contributor connections are currently installed by resource load-fns.
   Before a resource is materialized, the project aggregate does not know that
   contribution exists and can return an incomplete collision-group set.
2. If all candidate shells were connected to the aggregate and their summary
   outputs required materialization, the first collision-group evaluation would
   materialize every contributor. That is correct but creates a potentially
   large first-use fan-out.

Potential treatments, in increasing order of retained laziness, are:

- keep contributor-producing resources eager initially, including owners of
  embedded collision objects;
- register candidate shells up front and materialize all contributors when the
  complete aggregate is first requested;
- extract a lightweight collision-group summary per candidate resource and
  connect that shell-safe summary to `Project`, without creating editable
  substructure;
- define the aggregate as materialized-resources-only, accepting that ids and
  colors can change as resources load.

The materialized-only policy is unlikely to preserve current editor behavior.
The lightweight summary is the strongest long-term option; the eager policy is
the safest first implementation. Merely routing the child value through its
owning shell would clarify the ownership boundary but would not solve aggregate
completeness or first-use fan-out by itself.

### Overrides make dependency order mandatory

Collection, game object, and GUI property setters frequently create overrides
of referenced resource subgraphs and immediately query outputs such as
`:component-ids`, `:go-inst-ids`, `:node-ids`, or `:_properties` to restore
stored overrides. If the referenced resource is only a shell, the traversal is
empty or incomplete and overrides can be lost.

Consequently, it is not enough to load only the resource whose output was
requested. The coordinator must read its source value, recursively discover
structural dependencies through `:dependencies-fn`, and load that closure in
dependency order before running the requested resource's load-fn. The current
initial-load sorter should be reused rather than reimplemented.

Any output evaluation of an unloaded dependency encountered from inside a load
transaction should be treated as a missing dependency declaration, not by
starting a nested transaction.

## Why a load-fn must not run inside an output producer

An evaluation context contains an immutable basis and caches values against
that snapshot. Materialization changes the basis by adding nodes, properties,
connections, overrides, and invalidations. If an output producer transacts and
then continues:

- the producer still sees the pre-load basis;
- local/global cache hits belong to the old graph;
- newly-created substructure is invisible;
- callers that supplied an evaluation context can observe a mixture of old and
  new graph state;
- dry-run evaluation, tracing, and property-setter transaction contexts become
  unsafe;
- nested or concurrent loads can generate duplicate subgraphs.

This rules out the seemingly simple implementation of calling
`ensure-resource-node-loaded!` from a production function or from
`NodeImpl.produce-value` and then continuing the same evaluation.

## Proposed design

### 1. Stable shells and explicit load policy

Continue creating concrete nodes of each resource's registered `:node-type` so
that output schemas, type checks, resource lookup, and deterministic root
node-ids remain available without loading.

Introduce a resource-type load policy, initially opt-in:

- `:eager`: materialize during project open;
- `:on-demand`: create only the shell during project open;
- `:excluded`: preserve `.defunload` policy and do not automatically load from
  an ordinary output request.

Keep a small set of shell-safe identity outputs that must never trigger loading:

- `:_node-id`
- `:resource`
- `:node-id+resource`
- materialization-state inspection

`Project :nodes-by-resource-path` depends on `:node-id+resource` for every
resource, so treating it as a materializing output would immediately load the
entire project. Other outputs should materialize by default. Resource types may
explicitly declare additional unloaded-safe outputs only where their producers
are designed for it, as with today's `:allow-unloaded-use` behavior.

The barrier must run before cache lookup. Values produced from a shell must not
become valid cached answers for the materialized node.

### 2. Materialization coordinator

Add one project-level operation conceptually shaped like:

```clojure
(ensure-resource-nodes-materialized! project resource-node-ids options)
```

It should:

1. Resolve current node incarnations and ignore already-materialized or
   defective nodes.
2. Single-flight concurrent requests for the same nodes.
3. Read `node-load-info` for requested nodes.
4. Recursively read structural dependencies reported by `:dependencies-fn`.
5. Sort the closure with the existing load-order logic.
6. Merge source values and disk hashes.
7. Apply `node-load-info-tx-data` for the closure in one non-undoable
   transaction, including transpiler and save-data connections.
8. Prime important save-data cache entries as the eager loader does.
9. Mark failed reads/loads as materialized and defective so evaluation does not
   retry forever.

A state such as `:unloaded`, `:loading`, and `:loaded` is useful for diagnostics,
but `:loading` should be coordinated outside ordinary graph output production.
The committed graph can continue to use a boolean materialized property plus
the existing defective state if desired.

### 3. Evaluation barrier and load-aware boundary

The graph layer should know only that a node/output requires materialization;
it should not depend on editor resource namespaces. This could be represented
by inherited node-type metadata or a general evaluation-barrier callback.

When a non-shell-safe output is requested from an on-demand shell, the barrier
raises a typed materialization request before output cache lookup or argument
production. The request bubbles through nested input evaluation.

A load-aware evaluation boundary then:

1. Performs a dry-run/preflight evaluation with an empty cache to discover
   unloaded resource barriers reachable from the requested endpoint.
2. Calls the project materialization coordinator for the discovered roots and
   their structural dependency closures.
3. Repeats preflight because newly materialized subgraphs may reveal further
   resource dependencies.
4. Creates a fresh evaluation context from the new basis.
5. Evaluates the requested endpoint normally and commits its cache entries.

The flow is:

```text
endpoint request
    -> preflight on current basis
        -> no barriers -> evaluate and cache
        -> barriers found
            -> read dependency closure
            -> commit non-undoable load transaction
            -> preflight again with a fresh basis
```

Preflight avoids retrying arbitrary caller code or partially executing an
output tree. A barrier in real evaluation remains valuable as a correctness
guard for call sites that bypass preflight.

The largest API issue is caller-supplied evaluation contexts. There are many
editor call sites that intentionally share a context across several
`g/node-value` calls. A load cannot silently replace that context. We should
introduce a load-aware endpoint evaluation API and use it at top-level editor
boundaries. If a barrier escapes while using a caller-supplied context, fail
with a diagnostic until the caller has been migrated; returning a value from a
new private context would create inconsistent snapshots.

### 4. Eager bootstrap set and lightweight indexes

Some resources participate in project-wide services merely by being loaded.
Candidates for the initial eager set include:

- `game.project` and resources that define project settings/profile inputs;
- editor/plugin/transpiler configuration;
- resources contributing meta-info used to define property schemas;
- scripts needed for complete LSP/module/annotation indexes;
- collision-group contributors until their global summary is decoupled.

Prefer replacing eager materialization with lightweight indexes built directly
from resource source data. Examples include dependency paths, collision-group
names, Lua module/annotation summaries, and search data. Such indexes should not
create editable substructure or connect save data.

### 5. Resource sync behavior

For a never-materialized shell changed on disk, resource sync should update or
replace the shell without reading its contents. The next materialization reads
the latest resource. For a materialized node, retain today's replacement,
override transfer, outgoing-arc transfer, source-state, and cache behavior.

The existing `.defunload` branches in `read-nodes` and
`perform-resource-change-plan` provide much of this distinction. Automatic
on-demand loading should generalize the state handling while keeping `.defunload`
as an explicit user policy.

## Alternative approaches

| Approach | Advantages | Main drawbacks |
| --- | --- | --- |
| Explicit loading at open/build/edit entry points | Lowest graph risk; easy prototype; progress UI can be shown before work | Does not guarantee "any output" semantics; hidden evaluators can still see shell defaults |
| Barrier plus load-aware, restartable endpoint evaluation | Matches the requested semantics; works for nested graph dependencies; preserves fresh-basis evaluation | Requires graph support and migration of shared/caller-supplied evaluation contexts |
| Throw from `produce-value`, transact, and retry immediately | Small apparent surface area | Unsafe with snapshot contexts, caches, nested transactions, and side effects; not recommended |
| Full `ResourceNode` façade with only root-to-root cross-resource arcs | Strong encapsulation; best foundation for future unloading/eviction | Large refactor: many resource-specific inputs/outputs, overrides, and global aggregations cross current boundaries |
| Per-resource unloaded producers (`:allow-unloaded-use` everywhere) | Can serve build/search data without editable subgraphs | Requires bespoke producers for many outputs and does not provide general materialization semantics |

The explicit-boundary approach is a useful first delivery stage, while the
barrier/restartable-evaluation approach is the recommended end state. Full
façade encapsulation is complementary long-term work rather than a prerequisite
for initial first-load deferral.

## Suggested implementation stages

### Stage 0: Instrument and codify invariants

- Add diagnostics for resource materialization reason, duration, dependency
  closure, and triggering endpoint.
- Add a topology audit using explicit arcs and explicit cascade ownership.
- Reject explicit substructure-to-substructure arcs across resource owners.
  Report substructure-to-foreign-shell arcs separately and report
  override-propagated arcs in their own category.
- Inventory every connection from a resource shell or its substructure into an
  ownerless/global node, including whether it is installed at connect time or
  load time, and excluding the temporarily out-of-scope `:save-data`
  registrations.
- Detect global relay paths where these values are redistributed to other
  resource shells or substructure.
- Record project-open resource reads, materialized roots, node counts, and
  accidental fan-out.

### Stage 1: Generalize the loader and create shells

- Extract a public, idempotent materialization coordinator from the existing
  `.defunload` `ensure-resource-node-loaded` path.
- Split initial project loading into shell creation plus materialization of an
  eager policy set.
- Rename the current resource-type `:lazy-loaded` option to avoid semantic
  ambiguity.
- Opt in a small group of resource types without project-wide side effects.

At this stage, materialize explicitly before opening a resource, creating an
override of it, or building from it.

### Stage 2: Introduce evaluation barriers

- Add shell-safe output metadata and a typed materialization request.
- Add load-aware endpoint preflight and fresh-context evaluation.
- Migrate top-level view, outline, build, preview, and editor-extension
  evaluation boundaries.
- Treat a materialization request inside an active load/property-setter
  transaction as a dependency-declaration error.

### Stage 3: Remove eager project-wide effects

- Replace collision-group subnode registration with a lightweight complete
  index or another explicitly defined policy.
- Separate search values from graph materialization where practical. The
  current default search path evaluates `:save-value` for every textual
  resource and would otherwise materialize most of a project after a search.
- Decide whether LSP/script intelligence requires eager source indexes or may
  grow as resources materialize.
- Reduce the eager allowlist based on measurements.

### Stage 4: Consider eviction

Only after first-load deferral is stable, investigate unloading resources that
are no longer referenced by views, overrides, undo, or build state. This likely
requires the stronger root façade and explicit lifetime/reference tracking.

## Validation plan

Correctness coverage should include:

- shell creation without invoking read-fn or load-fn;
- shell-safe outputs that do not materialize;
- first materializing output loads once and returns the eager-load-equivalent
  value;
- nested and cyclic resource dependencies;
- collection/game-object/GUI overrides and restoration of overridden
  properties;
- missing/invalid resources becoming defective without retry loops;
- concurrent requests for the same resource;
- save, dirty state, undo/redo, migrations, and cache priming;
- external change, move, delete, and recreation before and after
  materialization;
- build, bundle, hot reload, preview, search, LSP, editor extensions, and project
  settings;
- deterministic node ids and collision-group behavior;
- the refined isolation audit on `save_data_project`, including zero direct
  cross-owner substructure arcs, the connect-time/load-time global-registration
  inventory, and an explicit policy for every global relay;
- `.defunload` policy remaining distinct from automatic on-demand loading.

Performance evaluation should compare project-open wall time, number of files
read, graph transaction time, total node count, cache population, retained
memory, first-open latency, and the size of cascaded materialization closures.
First build and first global search should be measured separately because they
are expected to materialize much more of the project than project open.

## Open questions

- Which outputs beyond the four identity outputs are safe on a shell?
- Which resource types form the minimum eager bootstrap set?
- Must project-wide collision-group and LSP indexes be complete immediately, or
  may they grow as resources materialize?
- Should dependency discovery data be cached independently of editable graph
  state to avoid rereading source files?
- How should progress and cancellation work when evaluation originates on the
  UI thread?
- Do editor extension APIs expose evaluation contexts in ways that require a
  compatibility layer?
- Is a barrier best represented as node-type metadata, an evaluation protocol,
  or a callback stored in the project graph?

The most important prototype question is whether a load-aware endpoint can
materialize a GUI/collection dependency closure, recreate a fresh evaluation
context, and produce exactly the same outline, scene, properties, build targets,
and override graph as eager loading. That exercise will validate the central
architecture before broad call-site migration.
