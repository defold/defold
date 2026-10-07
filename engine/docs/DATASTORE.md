# Data store

## File Format

A blob contains multiple component tables and one shared string area. Version 1
starts with an **eight-byte `DataFileHeader`**: FOURCC `DMDT`, followed by a
little-endian uint32 version. `DataFileDirectory` contains the table count and
string-area offset; an array of uint32 table offsets follows it. References are
blob-relative offsets; runtime pointers, group IDs and row IDs are not saved.

Each table stores its type hash, tags, field metadata, component name hashes,
fixed-stride byte rows and any dynamic payloads. **Metadata is shared by all rows
in a table**: a field has a full-name hash, kind and absolute row byte offset; an inline struct
also identifies its child metadata and size. Rows contain only values and
alignment padding. Serialization saves current values as the next load's defaults.
The fixed layouts are declared in the private [data.h](../data/src/data.h):
`DataTableHeader` describes each table, and `DataFileFieldMeta` describes each
field. These occupy 24 and 32 bytes respectively. Field metadata also retains
the local member hash for constructing or reading nested values. Counts and indices into metadata use uint16:
a type allows at most 65,535 metadata entries including nested members, and a
table allows at most 65,535 tags. Row counts, byte sizes and offsets remain uint32.
The reader and writer use these structs' sizes and member offsets.

![File overview at left; struct layouts at right show byte offsets, field names
and types for the header, directory, table header, field metadata and a
PointLight row. Solid arrows show offset and index references; dashed arrows show
layout expansion and sizes. Offsets are relative to each struct.](images/datastore-binary.svg)

### Example: PointLight and SpotLight

Both types contain a **`Light light`** member with `color` and `intensity`.
PointLight adds `range`; SpotLight adds `range`, `inner_cone_angle` and
`outer_cone_angle`. Light is embedded directly in each row, with no separate
pointer, row ID or lifetime.

The fields and defaults come from the existing [point-light](../../editor/resources/templates/template.point_light)
and [spot-light](../../editor/resources/templates/template.spot_light) templates.
Those templates are currently flat and represent color as a list. This example
shows the intended composition and explicit conversion of color to Vector3;
the producer conversion is not implemented yet.

| Layout | Members and byte offsets | Size / alignment |
| --- | --- | --- |
| Light | `color: Vector3` at 0; `intensity: Number` at 16 | 24 / 8 bytes |
| PointLight | `light: Light` at 0; `range: Number` at 24 | 32 / 8 bytes |
| SpotLight | `light: Light` at 0; `range: Number` at 24; `inner_cone_angle: Number` at 32; `outer_cone_angle: Number` at 40 | 48 / 8 bytes |

![PointLight and SpotLight embed the same Light layout. Their metadata describes
the members once; every row contains its own color, intensity and other values.](images/datastore-composition.svg)

A producer can reuse one `DataStructDesc` for Light. Each containing
table compiles its own metadata, shared by that table's rows. `light.color` is
at `offset(light) + offset(color)`; its offset can differ between containing
types. The producer supplies local text names for inline parents and members in
`DataFieldDesc::m_Name`; registration hashes each complete name, such as
`light.color`, and stores its absolute row offset. Registration and loading reject
duplicate full-name hashes across the layout, including inline members.
Text names are not retained.
Embedding Light does not make SpotLight a standalone Light component or
assign it a tag. There is no full schema system beyond these layouts.

### Value layout and alignment

Member offsets are **bytes**, not float indices. Align each member, align an
inline struct to its most-aligned member, and pad its size accordingly. Apply
this recursively, including trailing padding in the row stride. The producer
uses target-compatible `offsetof`/`sizeof` or equivalent offsets and sizes.

| Value | Stored bytes | Alignment |
| --- | --- | --- |
| Number | 8 (`double`) | 8 |
| Boolean | 1 | 1 |
| Vector3 / Vector4 / Matrix4 | 12 / 16 / 64 (`float32` components) | 4 |
| String / dynamic struct / list reference | 8 (blob offset) | 8 |
| Declared inline struct | Its members and padding | Maximum member alignment |

Matrices are column-major. The caller's blob buffer and table row starts must be
eight-byte aligned. Loading validates layouts and rejects unaligned buffers or
unsupported versions. Aligned scalar/math bytes permit native typed pointer
access on the engine's little-endian targets. Set changes a value, never its
declared kind.

The recursive kinds in [data_ddf.proto](../gamesys/proto/gamesys/data_ddf.proto)
remain supported, including null. Declared structs live inline; lists and structs
without a fixed member layout use dynamic payloads. A `DataFileContainerHeader`
stores the payload size and child count, followed by struct member names (structs
only), child kinds, offsets and payloads. Their children may differ between rows,
with a maximum of 64 container levels including enclosing inline structs. Equal strings share bytes in
the blob's final string area. String/container accessors resolve offsets rather
than treating reference slots as native pointers.

## Runtime Format

A **datastore** holds runtime tables, stable row IDs, resource registrations and
queries for many game objects. A **table** groups rows with the same type, tags
and field layout. Each data component occupies one row. A game object can have
multiple data components of different types, or several of the same type; they
can share its `DataGroupId`. A group is a caller-defined identifier stored on each
row; it may also identify a wave or other collection of rows. Different groups
share the same type table. Each row has a runtime `DataId`; loaded components also
retain their prototype-local component name hash.

`DataTable` holds runtime storage and ownership, separate from the file's
`DataTableHeader`. `DataValue`, `DataStruct` and `DataList` are temporary input or
borrowed views; they are not serialized as C++ structs.
Decoded insertion checks kinds and nested member names before adding any rows.
Nested scalar members use shared metadata directly; only structs, lists and
strings enter recursive validation or payload storage. Member order may vary.
Fixed numeric values copy directly into their declared byte slots; strings and
dynamic containers need separate payload storage. Bulk insertion clears and copies
row bytes per batch, then fills row identities and reuses released ID slots.
The store counts reusable slots, so reservation does not walk the free list.
Generation-exhausted slots are retired and excluded from that count.
ID-based typed getters first probe an immutable 16-bucket index of `uint16_t`
metadata indexes. They verify the full name hash and scan on collisions. This
costs 32 bytes plus one metadata pointer per runtime table, shared by all rows,
with no extra allocations or changes to the file format.

For construction without a blob, public `DataRegisterTable` copies a native C
layout and its tags. `DataCreateRows(store, type, group, count, rows, out_ids)`
copies complete native rows, including inline members, using the registered row
size. One group is assigned to every row; each receives its own ID. Native rows
have no component name. The caller prepares defaults and per-instance values.
Group membership does not imply ownership or deletion. Supplied values become
reset defaults, kept in the same dense row order as current values. Swap removal
moves both rows together and reuses the removed default slot. Inputs live only
through the call. Native reference fields use
`DataReference`: a string pointer, `DataStructInput` (member layout plus native
bytes), or `DataListInput` (typed array or mixed payloads). These eight-byte
input slots work inside inline structs too. Creation measures payloads for the
whole batch, reserves arena storage for strings/lists, and copies dynamic struct
fields into owned child tables. It does not retain input pointers or deduplicate
equal strings. String/list arena blocks remain allocated until the table becomes
empty or is destroyed; fixed row capacity remains available for later insertions.
List inputs can use one shared element kind and a contiguous native array, or
per-element `m_Types` with `m_Values` holding payload pointers. Mixed lists support
all recursive DDF kinds and the runtime math types; both forms store a kind per
element. Struct elements can each describe different members. `DataValue` and
serialization stay internal.

`DataCreateRowsSoA(store, type, group, count, field_count, fields, out_ids)`
accepts separate native field arrays. Each `DataFieldArray` pairs a root field
hash with `count` values; supply every root field once in any order. Inline
structs use arrays of complete structs. Field names resolve once per batch, then
values copy directly into the table's byte rows without a temporary AoS buffer.
Types and sizes come from the registered layout. Ownership, reset and locking
rules are the same as for `DataCreateRows`; this changes input layout, not storage.

### Owned dynamic structs

A dynamic struct field is an eight-byte **child-table index and row index**.
Within a component table, objects with the same member hashes and kinds share
one `DataStructTable`, regardless of input member order. Its metadata supplies
C-aligned offsets; its fixed-stride rows contain only values. Nested struct
members use the same representation recursively. These are private storage
tables, with no public `DataId`, tags or separate query membership. Inline
structs such as `Light` above remain in the component row.

![An Enemy row owns a dynamic attributes row. Two attributes rows share metadata
but have separate values. Freed child slots are reused without moving another
parent's child.](images/datastore-owned-structs.svg)

Each child row belongs exclusively to one parent. Reusing an input pointer makes
independent copies, not shared mutable references. Reset keeps the original tree
and releases any replacement tree; removing the parent releases both. Free slots
are linked through their unused row bytes and reused by later insertions. Child
rows do not move, so removal needs no global ID lookup or reference fixups.
Capacity and shared layouts remain allocated until the component table is freed.
Strings and lists inside child rows still use the containing table/registration's
payload arena and follow its reclamation rules.

This applies to owned dynamic struct **fields**. Mixed lists, including their
struct elements, keep the packed container representation. Loaded dynamic
structs continue to reference immutable blob payloads directly; an explicitly
replaced struct uses child rows, and reset returns to the blob. Serialization
writes current child values into the existing version-1 container format. There
is no change to the zero-copy blob loading path.

For scattered reads, public `DataFieldGet*Batch` functions accept an ID array and
one field hash. They stage addresses before copying values, with no allocation.
The count follows the store argument, and output values follow input order. One
`DataResult` reports success or the first error. Outputs may be partially written
on failure; consume them only after `DATA_RESULT_OK`. The caller provides stable
storage and synchronization, just as for scalar getters. The library chooses its
internal batch size.

The resource system owns the immutable blob allocation. `DataLoadBlob` validates
and borrows it without copying or changing the bytes; loading alone adds no rows
to a datastore. A store retains references to loaded blobs through its resource
pools. Registrations of the **same loaded blob** append mutable rows to the same
runtime tables and reuse query bindings. Different loaded blob handles remain
separate, even if their layouts match.

![The datastore references caller-owned blobs. Each resource pool has shared
runtime tables and registration records connecting game objects to their rows.](images/datastore-tables.svg)

### Adding and removing game objects

`DataAddBlob(store, blob, group, &instance)` instantiates all component rows from
the blob in one operation. It copies their fixed-size value bytes into the
resource's shared dense tables once. Metadata and defaults remain in the blob;
strings and dynamic containers initially reference its payloads. Pooled
registration records and membership indices track the new rows. Capacity grows
as needed and is reused by later registrations.
Each runtime table records whether its rows contain references. Numeric-only
instantiation and reset copy the bytes directly without walking field metadata.

![Load a resource once, then register game objects A and B. Each registration
appends a point-light row and a spot-light row to the existing runtime tables.](images/datastore-runtime.svg)

Numeric and math writes modify these mutable rows directly, without first-write
allocation or a second private copy of defaults. Replacement strings/containers
belong to the registration. Runtime reference slots distinguish blob offsets,
owned string/list pointers and owned child-row indices; file bytes remain unchanged. Rows added without a blob
retain table-owned defaults instead.

Reset restores loaded/added values: `DataResetRow` targets one component,
`DataResetBlob` targets one registration's remaining rows, and field reset
targets one value. IDs survive reset; added/removed rows are not undone. Resetting
one game object's registration does not affect another. Component/field reset
releases replacement child rows but keeps string/list payload blocks; complete
registration reset also frees those blocks.

`DataRemoveBlob` removes the registration's remaining rows, invalidates their IDs
and frees replacement payloads. Dense removal can move another row; its ID remains
stable. Shared capacity remains until the resource pool's last registration is
removed. The caller may free the blob buffer only after all registrations and the
loaded blob reference have been released.

Optional write protection belongs to the resource owner: a dedicated, page-aligned
mapping could be made read-only with `mprotect` / `VirtualProtect`. This is not
implemented by `DataLoadBlob`; it must not change protection on arbitrary caller
allocations. Mutable runtime rows remain in separate writable storage.

**Engine integration:** the intended `.goc` packaging is one associated `.datac`
blob containing all of a prototype's data-component tables, loaded once and
instantiated for each game object. Grouped blobs and the runtime operations above
exist in `engine/data`. Bob still writes individual DDF `.datac` resources;
aggregation and gamesys resource/game-object integration remain future work.

## Queries

A query describes required table tags and full field-name hashes with exact kinds, plus
optional group filters. All requested fields must belong to the same table;
queries do not join separate component rows. For example, require the `light` tag
and `light.color` as Vector3 to visit both light types above, provided their tables
were assigned that tag. Value tests, such as a radius check, run in the row loop.

![Create a light-color query, cache a binding for each matching table, then reserve
access and iterate batches and rows using a stable field handle.](images/datastore-queries.svg)

1. **Create once:** `DataCreateQuery` copies a `DataQueryDesc`. For `light.color`,
   its `DataQueryField` uses `m_Field = dmHashString64("light.color")` and
   `m_Type = DATA_TYPE_VECTOR3`. No path array or runtime string parsing is needed.
2. **Bind once:** `DataQueryFindField` returns a handle for that full-name hash and
   kind. Each matching table caches its own byte offset under the handle. Missing
   or incompatible fields exclude a table. Adding/removing tables updates
   matches without invalidating the handle.
3. **Reserve access:** `DataQueryTryBegin` returns OK when the requested access is
   available, or BUSY so the caller can retry. Query creation itself reserves nothing.
4. **Iterate:** `DataQueryGetRowCount` counts reserved matching rows.
   `DataQueryIterRange(query, first, count)` creates a cursor over a selected range;
   `DataIterNext` selects a batch, and `DataIterRows` / `DataRowIterNext` traverse
   its rows. `DataRowIterGetVector3(&rows, color_field)` returns the current color.
5. **Release:** after all iterators and borrowed pointers finish, call
   `DataQueryEnd`. Reuse the query on the next update; destroy it when no longer needed.

A field handle is neither a byte offset nor an index in the query descriptor.
Each query match stores a compact array of uint32 byte offsets alongside its bindings. Typed pointer getters calculate
`batch base + row × stride + field offset` inline, without copying, allocation,
hash lookup or repeated kind checks.
Writable getters such as `DataRowIterGetNumberMut` require a field declared with
`DATA_ACCESS_READ_WRITE`; read-only access is the default. Number, Boolean,
Vector3, Vector4 and Matrix4 have typed pointer accessors.

Dynamic container members cannot bind because their layout may differ per row.
ID-based access uses typed functions such as `DataFieldGetVector3` and
`DataSetFieldVector3`, using the same full-name hash for inline members.
Generic `DataValue` is internal.

Cursors allocate nothing and borrow their parent's current batch/row. Do not copy
active cursors; finish using children and pointers before stepping a parent,
including on END. Parents must remain alive at the same address. There are no
per-row lifetime checks. Field handles last until query destruction; borrowed
value pointers must also be reacquired after mutations affecting their storage.

### Parallel access and structural changes

The library provides synchronization; the caller owns threads and jobs.
`DataQueryTryBegin` holds the store mutex while checking conflicts, refreshing row
ranges and recording an active reservation. It then releases the mutex, but the
reservation remains active throughout iteration. Structural operations acquire
the same mutex and return `DATA_RESULT_LOCKED` while any reservation is active,
preventing row storage from moving or being freed. The caller finishes all jobs
before `DataQueryEnd` removes the reservation under the mutex. Structural changes
can succeed after the last reservation ends; rejected operations must be retried
by the caller.

There are no per-table mutexes. Reservations allow concurrent access to different
tables or non-overlapping fields in the same table.
Read/read overlaps are allowed; overlapping writes conflict. Reading a whole
inline struct conflicts with writing a member. Group filters do not narrow these
reservations. Queries without requested fields reserve reads of complete rows.

Explosion reads position and writes health. Regenerate also writes health,
increasing it up to 100. Whichever reserves access first runs; the other receives
BUSY and retries after release. Independent queries can proceed concurrently.
Each query handle has at most one active reservation, shared by its jobs.
Active reservations use `dmObjectPool` slots: admission scans a dense array and
release returns the query's slot directly. Query creation reserves pool capacity;
begin/end reuse it. This follows the instance-pool pattern in
[rig.cpp](../rig/src/rig.cpp). Query descriptors require valid caller storage; lookup does not repeat pointer validation.

![The caller submits jobs for admitted queries, waits for them, then releases
access. Conflicting updates retry; structural changes wait until queries end.](images/data-query-execution.svg)

Split the reserved row count into disjoint ranges, and give each job its own
`DataQueryIterRange` cursor. A range can split or cross table batches; it is not an
ID range or thread number. For 8,500 matches and 4,096 rows per job, submit
`(0, 4096)`, `(4096, 4096)` and `(8192, 308)`. The first begin after structural
changes rebuilds cached ranges and may allocate; later begins reuse them. Join all
jobs before `DataQueryEnd`, including when there are no matches. See the
[submission loop and threaded benchmark](../data/README.md#threaded-update-benchmark).

Structural operations, query creation/destruction, serialization, ID-based writes
and resets reject changes while queries are active. String/container replacement
also stays outside reservations. Fixed-size fields need no per-row lock or atomic
operation. Unrelated ID-based access must not bypass query reservations.

The main thread may prepare resource blobs while jobs run, then stop admissions,
wait for queries to end and apply pending additions/removals. To remove rows found
during a scan, collect their `DataId`s and remove them afterward. Removal uses IDs
because dense row indices can change. Ending a query does not apply queued work.

For single-threaded traversal, `DataStoreLock` + `DataQueryIter` remains available;
unlock after traversal, including early exits. This guard holds the store mutex
on the calling thread and allows value writes/reset while rejecting structural
changes. Do not mix it with active query reservations.

**Concurrency follow-up:** structural changes currently require every query in
the store to finish, even when they affect unrelated tables. Investigate allowing
additions/removals on unreserved tables while keeping active row storage, ID slots,
query bindings and shared resource pools stable. Also profile query admission:
conflict scans and range rebuilding after structural changes run under the shared
store mutex. Keep field-level access reservations and caller-owned jobs; validate
any narrower synchronization with TSAN and concurrent spawn/despawn coverage.

## Implementation

The public [data.h](../data/src/dmsdk/data/data.h) is the umbrella header: existing
includes still provide the complete C API. It includes the responsibility headers
below and [data_types.h](../data/src/dmsdk/data/data_types.h) for shared handles,
IDs, results and value types. Each public header can also be included directly.
Store, native table/row lifecycle, reset and opaque blob handles are public.
Serialization and generic construction values remain in the private [data.h](../data/src/data.h).

| Implementation | Public header | Responsibility |
| --- | --- | --- |
| [data.cpp](../data/src/data.cpp) | [data.h](../data/src/dmsdk/data/data.h) | Store creation/destruction and locking. Native row lifecycle is declared in [data_table.h](../data/src/dmsdk/data/data_table.h); resource instances in [data_blob.h](../data/src/dmsdk/data/data_blob.h). |
| [data_query.cpp](../data/src/data_query.cpp) | [data_query.h](../data/src/dmsdk/data/data_query.h) | Query matching, field bindings, cached ranges and access reservations. |
| [data_iter.cpp](../data/src/data_iter.cpp) | [data_iter.h](../data/src/dmsdk/data/data_iter.h) | Batch, row and field traversal, including iterator value access. |
| [data_field.cpp](../data/src/data_field.cpp) | [data_field.h](../data/src/dmsdk/data/data_field.h) | Value representation, construction validation, field get/set and reset. |
| [data_io.cpp](../data/src/data_io.cpp) | [data_blob.h](../data/src/dmsdk/data/data_blob.h) | Public blob loading and release; private serialization. |

The [library and benchmark documentation](../data/README.md) covers measurements
and comparisons with Flecs, Bevy and EnTT. Threading tests run under TSAN;
performance measurements use separate optimized builds without sanitizers.
