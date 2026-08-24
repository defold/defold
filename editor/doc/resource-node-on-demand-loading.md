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

`game.project` is connected to project-wide settings before other resources are
loaded. This is an example of a bootstrap dependency that must remain explicit.

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

### The strong encapsulation invariant is not true today

A runtime audit of `test/resources/test_project` found:

| Item | Count |
| --- | ---: |
| Project resource roots | 488 |
| Total graph nodes after project setup | 1,894 |
| Resource roots with explicit cascade-owned substructure | 130 |
| Explicitly cascade-owned nodes below project resource roots | 876 |
| Explicit cross-owner arcs touching owned nodes | 1,081 |
| External/resource-root to owned-node arcs | 1,064 |
| Owned-node to external arcs | 17 |

This is a representative test project snapshot, not a performance benchmark.
The audit considered explicit arcs between non-override nodes and determined
ownership by following explicit `:cascade-delete` arcs. Override propagation
introduces implicit ownership arcs, so `owner-resource-node-id` alone is not a
reliable topology-audit primitive in a graph containing overrides.

The largest direct subgraphs in this project came from GUI scenes (333 nodes),
collections (245), atlases (93), game objects (78), and `game.project` (50).
This indicates meaningful memory and graph-transaction savings are possible
even before considering override-node expansion.

### Most cross-owner arcs do not prevent first-load deferral

The 1,064 incoming arcs are primarily normal dependency injection:

- image outputs feed atlas image nodes;
- atlas, tile source, material, and font outputs feed GUI, sprite, model, and
  particle substructure;
- game object and collection outputs feed referenced-instance nodes;
- project settings feed owned rendering/component nodes.

These arcs are created while the consuming owner is loaded. They do not need to
exist while that owner is a shell, and their deletion is naturally handled when
the owned target node is deleted. Therefore a practical first invariant is
weaker than complete encapsulation:

> An unmaterialized resource has no substructure, and a materialized resource's
> owned nodes must not publish explicit outputs to nodes outside the owning
> resource.

This one-way rule is enough for initial deferral. Full root-to-root encapsulation
would also forbid incoming arcs and would require a broad façade refactor.

### The outbound exceptions are project-wide collision groups

All 17 real outbound arcs in the audit came from embedded collision-object or
tile-source collision-group nodes feeding `Project :collision-group-nodes`.
This is a genuine global effect of materializing substructure.

With demand loading, the project-wide collision-group set would otherwise be
incomplete and its stable id/color allocation could change as more resources
are opened. Initial options are:

- keep collision-group contributors eager;
- extract a lightweight collision-group summary while reading resource headers;
- publish a summary through the owning resource root and define whether the
  project aggregate represents all resources or only materialized resources.

The second option gives complete project-wide behavior without retaining full
resource subgraphs.

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
- Enforce no owned-to-external explicit arcs for on-demand resource types, with
  temporary documented exceptions.
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
