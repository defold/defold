# Data store

A **datastore** holds runtime tables, stable row IDs and queries for many game
objects. A **table** groups component rows with the same type, tags and property
layout. Its shared metadata describes each property's name hash, kind and byte
offset within a row. Rows contain only value bytes; they repeat neither names nor
kinds. Tables can be added and removed at runtime.

## Game objects and tables

A game object may have several data components of different types, or several of
the same type. Each component occupies a row. Its component name hash identifies
it within the prototype; its `DataId` identifies that runtime row. All rows for
one game object share its `DataOwnerId`.

The intended `.goc` packaging is **one associated `.datac` blob containing all
of the prototype's data-component tables**, including nested values and a shared
string area. The resource system loads this immutable blob once and keeps it alive
while instances use it. Each registration gives a game object mutable copies of
its component rows, sharing the resource metadata and initial values. One
registration handle resets or removes that game object's component rows together.
A component reset addresses just its row ID, without affecting sibling components.
Registrations of the same loaded resource append rows to shared runtime tables.
Queries visit each matching table once, across all its registrations.

![One resource supplies two shared runtime tables. Each table contains rows from
multiple game objects; registration handles track their component rows.](images/datastore-tables.svg)

The examples use fields from the existing [point-light](../../editor/resources/templates/template.point_light)
and [spot-light](../../editor/resources/templates/template.spot_light) templates:
`color`, `intensity` and `range`; spot lights also have cone angles. A `light` tag
lets a query visit both types.

**Integration status:** Bob still writes individual DDF `.datac` resources.
Aggregation by the `.goc` builder and gamesys integration remain future work.
Grouped blobs, mutable-row instantiation and lifecycle operations exist in
`engine/data`; connecting these to game objects/resources remains integration work.

## Shared metadata and byte rows

For a point-light layout, metadata can declare `color` as Vector3 at byte 0,
`intensity` as Number at byte 16 and `range` as Number at byte 24. Each 32-byte row
then contains those values, including four padding bytes after color. Offsets are
**bytes**, not float indices; producers use C `offsetof`/`sizeof` or equivalent
aligned offsets and stride. Registration rejects unaligned layouts. Get/set finds
the name in shared metadata and accesses that offset. Every row uses the declared kinds; set rejects a different kind.

**Aligned layout (version 8):** align each member, give an inline struct the maximum
alignment of its members, and pad its size to that alignment. Apply this recursively; the row
stride includes trailing padding so every row remains aligned. The current format
types need 1-byte alignment for Boolean, 4 for float vectors/matrices, and 8 for
Number (`double`) and reference slots. Matrix4 occupies 64 bytes but needs only
4-byte alignment with its current plain-float definition.

The caller's blob buffer and table row starts must also be aligned (8 bytes covers
these types). The format fixes widths and alignment; compile-time checks verify
compatibility with native pointer types. Producers must use the target layout, not an unrelated
build host's ABI. For compatible little-endian targets, numeric/vector/matrix getters return pointers
to aligned mutable row bytes. Loading borrows the immutable blob; registration
copies fixed-size rows once. Both validate the layout before traversal.
File reference slots remain offsets rather than native pointers; string/container
accessors resolve them. Unaligned buffers/layouts and earlier file versions are
rejected. The format still uses only the magic and version in its eight-byte header.

**Optional resource protection (not implemented):** the resource owner could load
the whole shared blob into a dedicated page-aligned mapping, then make it read-only
with `mprotect(PROT_READ)` or `VirtualProtect(PAGE_READONLY)`. Accidental writes
would fault without adding checks to getters. Protection applies to whole OS
pages, so the allocation must use the runtime page size and share no pages with
mutable data; the format's 8-byte alignment alone is insufficient. Runtime metadata
and instance values remain in separate writable memory. `DataLoadBlob` only borrows
the buffer and must not change protection on arbitrary caller allocations. A
read-only file mapping is another option when the resource can be used directly.

![The blob contains one metadata block per table followed by fixed-stride byte rows.
The point-light example has Vector3 color and two double-precision numbers.](images/datastore-binary.svg)

Vector3, Vector4 and Matrix4 contain 3, 4 and 16 float32 components; matrices use
column-major order. Number is double precision. These are supported by C get/set
and serialization. C functions use the `Data` prefix. Typed accessors such as
`DataGetPropertyVector3` / `DataSetPropertyVector3` exchange a `DataVector3` value;
`DataFieldIterGetVector3` / `DataFieldIterSetVector3` use a cached query binding.
Number, Boolean, String, Vector4 and Matrix4 have corresponding accessors. They
require the declared kind and leave outputs/stored values unchanged on error.
Math types also describe the native values returned by typed pointer getters.
They do not expose the complete row layout. Generic `DataValue` and
container descriptors are internal engine types.

The recursive kinds in [data_ddf.proto](../gamesys/proto/gamesys/data_ddf.proto)
remain supported: null, number, boolean, string, struct and list. A struct with a
declared member layout lives inline in the row. Structs without that layout and
lists use reference slots and compact dynamic payloads; their children may vary
between rows. Strings refer to shared blob bytes until replaced. Replacements
belong to the registration (or table for independently added rows). Nesting is limited to 64 container levels.

The light templates currently express color as a list. The Vector3 layout above
requires an explicit producer conversion; DDF conversion is not wired up yet.
There is no full schema system beyond the table's field layout.

## Composition and queries

`SpotLight` can contain `Light light`, with common `color` and `intensity` fields
stored inline. The internal producer reuses a `DataStructDesc` when declaring
PointLight and SpotLight. Registration compiles that description into each
containing table's metadata once; all rows share it. Packed registrations borrow
the metadata from the blob. No member hashes, kinds or pointers to Light are
repeated in the rows. Light has no separate row ID or lifetime.

![A reusable Light description produces table metadata and inline rows.
Queries bind the combined light.color offset once per matching table.](images/datastore-composition.svg)

A query can require `light.color` with kind Vector3, optionally with a `light` tag.
`DataQueryProperty` supplies the root hash and member path. Matching resolves
`offset(light) + offset(color)` and the kind once per table; missing or incompatible
properties exclude the table. Requested properties have no field-order guarantee.

After query creation, call `DataQueryFindField` once for each required full path
and kind. Its small integer handle identifies that requested field for the query's
lifetime, even if no tables currently match. Each matching table caches its own
offset under that handle; adding/removing tables does not invalidate it. Handles
specify neither raw byte offsets nor field iteration order. Reuse them across all
batches and traversals with
`DataFieldGetVector3(&rows, field)` or `DataFieldGetNumber(&rows, field)`.
These return const pointers to the mutable values without copying,
allocation, name lookup, or repeated kind/lifetime checks. Boolean, Vector4 and
Matrix4 have the same API. Pointer access requires a little-endian host; copying
accessors retain endian conversion. Use the typed getter matching the bound kind.

`DataFieldGetNumberMut` and corresponding math/Boolean functions return writable
instance-owned values immediately, without allocation or copying. Nested writes
change only the selected member; siblings and reset defaults stay intact. Strings
and dynamic containers retain ownership-aware setters. Reset copies the loaded/added
values back into the existing mutable rows.

Keep the store locked during traversal. Handles last until query destruction;
pointers last only until the next row/batch step, unlock, or another mutating
access/reset affecting their row/table. Reacquire after mutation to observe the
current value. These are caller obligations, without per-row revision checks.
`DataFieldIterator` remains available for enumeration and copying accessors; it
visits requested fields, or all top-level properties for queries without them.
Dynamic container paths cannot bind because their layout may differ per row.

Embedding Light does not make SpotLight match a query for the Light component
type, nor add a tag automatically. Flecs and Bevy likewise query the containing
component, then use ordinary C/Rust member access. Their official query APIs
supply typed arrays/references: [Flecs queries](https://www.flecs.dev/flecs/Queries.html)
and [Bevy queries](https://docs.rs/bevy_ecs/latest/bevy_ecs/system/struct.Query.html).

Flecs distinguishes `ecs_get` (const, including inherited values) from `ecs_get_mut`
(writable, owned components only). Query terms declare read/write access separately;
the C `ecs_field_w_size` function still returns `void*`, so callers must respect
those declarations. Inherited prefab values need an owned override before an
instance can change them independently. See [Flecs access APIs](https://www.flecs.dev/flecs/group__flecs__c__getting__setting.html).

Separate Light and SpotLightParams components remain another design option:
Flecs/Bevy can place them in separate columns of the same archetype table and
iterate Light directly. A referenced Light would instead allow sharing or an
independent lifetime, with an extra lookup. Neither alternative is implemented
here. One game object may contain multiple lights, so such associations would
need component-instance identity as well as game-object ownership. All models
could still share one resource blob. The existing light templates remain flat.

## Reset scope

The normal engine scopes are **one component** (`DataResetRow(store, id)`) or
**one game object's registered data** (`DataResetBlob(instance)`). A property can
also be reset individually. Each restores loaded/added defaults, including nested
values, while preserving IDs and iterators. Resetting a game object leaves other
instances of the same prototype unchanged.

The engine retains component row IDs and registration handles, so reset needs no
store-wide owner lookup. Any independently added component rows or additional
registrations must be included through their retained IDs/handles. `DataResetTable`
spans owners of one independently registered type; it remains a bulk utility for
benchmark setup, not the normal game-object operation.

Component reset copies its defaults into the existing mutable row. It does not
free string/container blocks that may also hold other components in that
registration. Game-object registration reset or destruction releases those blocks,
even when its rows share physical storage with another game object.

## Resource loading and mutable instances

The resource owner reads the file into its allocation and passes
it to `DataLoadBlob`. Validation borrows that buffer unchanged, without copying or
fixing up file bytes. Resource loading stays separate from game-object creation.

Instantiation copies the fixed-size row bytes into shared dense tables once.
Repeated registrations of a loaded resource use the same runtime tables and query
bindings. Registration handles and row membership indices use reusable pooled slots;
row arrays grow geometrically. Metadata remains shared. Strings and nested
containers initially reference the resource; replacements belong to the registration.
Numeric and vector/matrix get/set access the mutable row directly. There is no
first-write row allocation or per-property default/override selection.

![Load one immutable resource, copy row bytes when creating an
instance, write in place, and restore defaults from the resource on reset.](images/datastore-runtime.svg)

Reset copies defaults for the selected property, component row, or game object's
remaining component rows back from the resource. It restores string/container
references too; resetting the complete registration frees its replacement payload
blocks. IDs survive reset, and row additions/removals are not undone. Instance
destruction removes its rows, frees replacement payloads and recycles its handle slot.
Shared row/registration capacity stays available until the last registration of
that resource is removed; then the pool is released. The resource stays alive while
any registration references it. No instance needs a second
private copy of the resource defaults; source table/row indices locate them for reset.

Row allocation/copying occurs at instantiation; scalar/math writes allocate nothing.
Untouched instances therefore use mutable memory up front. Rows added through the
API without a resource retain table-owned default bytes as their reset source.

Runtime string/container slots distinguish shared blob offsets from aligned owned
payload pointers with a low-bit tag. This applies only to the mutable copy; the
file stays unchanged. Inline member references are tagged during instantiation or
reset, and dynamic children keep their original relative offsets. Replacing one
member leaves other shared payloads in place. Numeric fields have no reference
encoding or per-property changed bits.

The C20 measurements include the mutable row copies in creation/registration and
measure allocation-free numeric writes. Flecs/Bevy also write existing mutable
components; Defold additionally retains reset defaults. Their reset source remains
fixture data outside the measured store.

## File and API boundaries

Version 8 uses an eight-byte header: FOURCC `DMDT` and a little-endian uint32 version.
A table directory follows. Each table has shared metadata, component name hashes,
fixed-stride rows and nested payloads. All tables share the final string area.
References are blob-relative offsets. Runtime owner IDs, row IDs and pointers are
not saved. Serialization saves current values as the next load's reset defaults.

The first registration allocates shared table objects, initial rows and one page
of registration records together. Later registrations reuse capacity; handle pages
hold up to 256 records and usually stay within 64 KiB. A larger registration gets
one record per page. Shared store registries and query caches can grow separately.
Different loaded resource handles remain separate, even with identical layouts;
sharing across resources is not implemented. An array reaching its size limit
starts another pool.
The [C20 file-load measurements](../data/benchmarks/file-loading.csv)
still use five allocation requests into a fresh store and three with reused
capacity, including the caller's file buffer. These measure the current
implementation, including mutable row instantiation.

The public SDK provides queries, iteration, structural lock/unlock and get/set. Store/table/row management,
reset and serialization are internal. Queries can require property names and
exact kinds, including declared inline member paths, caching each field's offset
per matching table. `DataIterator` visits batches, `DataRowIterator` visits their
rows, and `DataFieldIterator` visits the requested fields (all top-level fields if
none were requested). The field cursor exposes its index; `DataFieldIterGetType` and
`DataFieldIterGetNameHash` read shared metadata on demand;
`DataFieldIterGetNumber(&field, &value)` reads the current field without index
arguments. Cursors allocate nothing and borrow their parent's current batch/row.
Do not copy active cursors; finish using children before stepping a parent, including
on END. Parents must remain alive at the same address. These are caller lifetime
obligations, with no per-row validation. Hold `DataStoreLock(store)` for traversal
and call `DataStoreUnlock(store)` afterward, even on early exits. Locks nest and
allocate nothing; structural mutations return `LOCKED` until the outermost unlock.
Value writes/reset remain allowed. This guard does not synchronize threads.

To remove rows while scanning, collect their `DataId`s in a reusable caller-owned
buffer and remove them after unlocking. Queued rows remain visible until then.
Use IDs because swap-removal changes row indices. Unlock does not apply pending
work automatically. Hash-based reads
scan shared metadata; there is no raw column view. Fixed member paths avoid this
search during iteration; dynamic container traversal remains separate.
Flecs' dense tables instead group entities by component set and store a column per
component identity. The [benchmarks and format details](../data/README.md) record
the current costs and remaining limitations. The current light-color,
health/position and Explosion benchmarks resolve field handles during query
creation and reuse them with typed pointers across all batches. Each C++ case
defines its query in the same source file as its traversal. Explosion uses its own
health/position query; the radius test remains in the row loop. Query creation is
measured separately, with five dense queries and two resource-instance queries. The C21
[comparison](../data/benchmarks/comparisons/c21-case-queries/RESULTS.md) measures
this setup.

## Proposed parallel access

This design is not implemented. The current `DataStoreLock` remains a
single-threaded structural guard. The data library will not create threads,
schedule jobs or depend on the job system. Callers may use the engine job system
(`HJobContext`), another executor, or ordinary sequential calls.

A query declares read or read/write access for each requested property, including
inline member paths. Read is the default; an Explosion query reads position and
reads/writes health. Regenerate requests read/write health and adds 0.25, capped
at 100. It conflicts with Explosion on their shared tables. The caller specifies
Explosion before Regenerate when it needs that order; rejecting simultaneous
access alone does not establish update order. A query without an explicit
property list conservatively declares access to entire matching rows. A separate
execution scope reserves the query's access and keeps its matching storage and
bindings stable until release.
Query creation alone acquires no access.

Beginning an execution checks active reservations under a short mutex. Two
executions conflict when they match the same physical table, their field byte
ranges overlap, and at least one writes. Reading an entire inline struct therefore
conflicts with writing one of its members. Property hashes or different tags alone
cannot establish independence. Initially owner filters do not narrow reservations
within a table. Failed acquisition returns busy without retaining partial access;
the caller retries or orders jobs. The mutex is released before traversal, so
independent queries run concurrently without locks or atomics in row access.
Access declarations cover all data touched by the job; unrelated ID-based access
must not bypass reservations.

The caller splits one execution into nonoverlapping result ranges and creates a
separate iterator for each range. Ranges can end inside a table. The execution
reserves the whole query once; its jobs must finish before it is released. Range
indices exist only within that execution, are not persistent IDs, and do not
represent threads. `first` and `count` refer to matched rows in the execution's
complete result sequence, not table indices or IDs; a range may span table
batches. For 8,500 matches and a caller-selected chunk size of 4,096, the ranges
are `(0, 4096)`, `(4096, 4096)` and `(8192, 308)`. Each job constructs its own
batch/row cursors over its assigned range; jobs share the execution, never an
active iterator. The [submission example](../data/README.md#caller-side-job-submission-proposal)
shows the caller's scheduling and completion loop.
Read/write declarations are checked when binding writable
fields, rather than on every row. The first version permits direct fixed-size
value updates during execution; string/container replacement and reset remain
exclusive operations because allocation and payload ownership can be shared.

An engine-owned mutation phase applies content changes only after all active
executions finish. Admission and mutation exclusion use the same synchronization;
checking an active count and then mutating without exclusion would race. Query
creation/destruction, table/row registration/removal, resets and resource release
belong to this phase. The main thread may read and prepare new blobs while jobs
run, retaining pending loads/unloads in caller-owned queues. Queries see the old
content until changes are applied. Blob bytes stay alive through every execution
and registration that borrows them. The coordinator stops admitting new jobs when
it needs to apply changes; it must continue processing job completions while
waiting. Mutating unrelated live tables during execution is outside this first
design because slots, registration records and query lists are shared store state.

The proposed [threaded update benchmark](../data/README.md#proposed-threaded-update-benchmark)
exercises concurrent queries, caller-selected ranges and content changes across
these synchronization points.
