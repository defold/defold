# Data library

Experimental C API for queries, iteration and property get/set. Functions use the
`Data` prefix (for example, `DataCreateQuery`); the previous `dmData` names are
removed. The public header is `src/dmsdk/data/data.h`, installed as `<dmsdk/data/data.h>`. Store/table/row
management, reset, binary I/O and generic `DataValue`/container access are internal,
declared in `src/data.h` beside
`data.cpp`. Serialization is implemented in `data_io.cpp`. The library uses dlib
containers and does not depend on gameobject, gamesys, DDF or Flecs.

## Storage and lifetimes

Each table has a type hash, tags and shared property metadata: name hash, value
kind and byte offset within a fixed-stride row. Metadata is stored once per table,
not with every value. Decoded row inputs contain values in metadata order. The
layout fixes each top-level property's kind; get/set uses its name hash to find
the byte offset. Set rejects a mismatched kind and cannot add fields. There is no
full schema system for nested content.

| Kind | Row bytes | Representation |
| --- | ---: | --- |
| Null | 0 | No payload |
| Boolean | 1 | Zero or one |
| Number | 8 | IEEE-754 binary64 |
| Vector3 | 12 | Three float32 components |
| Vector4 | 16 | Four float32 components |
| Matrix4 | 64 | Sixteen float32 components, column-major |
| String, dynamic Struct, List | 8 | Blob offset or runtime reference |
| Declared inline Struct | Layout size | Embedded member bytes; metadata shared by rows |

A point-light layout can use Vector3 color at byte 0, Number intensity at byte 16
and Number range at byte 24, with a 32-byte stride. Four padding bytes align the
doubles. Version 8 requires C-compatible member alignment and row tail padding. `DataValue` and its union are internal construction/I/O values, never row storage.

Packed tables borrow metadata, tags and reset defaults from the caller's immutable
buffer. Registrations of the same loaded resource append mutable rows to shared dense
runtime tables. Registration handles and row membership indices use pooled slots. Strings and dynamic
containers keep referencing shared blob bytes until replaced; no payload trees
are expanded during registration. Each registration assigns fresh row IDs and the
supplied owner ID; a component name hash identifies each row within its prototype.
Owner IDs are opaque uint64 values and may be shared by multiple rows, including zero.

For decoded construction, `DataRegisterTable` copies metadata/tags once.
`DataAddRows` validates the batch, stores table-owned reset defaults and initializes
mutable rows. Row removal swaps both values and identities while preserving the
source row index for reset. Removed default bytes remain until table destruction.

Scalar/math reads and writes address mutable row bytes directly, without allocation,
changed-property masks or default/override selection. String/container replacement
copies only the replacement payload into registration-owned blocks (table-owned for
independently added rows), including self-assignment.
Nested fixed-member writes preserve the other members and their shared payloads.
Runtime reference slots use tagged blob offsets or aligned owned payload pointers;
numeric bytes and the immutable file representation are unchanged.

Reset follows engine instance ownership:

- `DataResetProperty(store, id, property)` restores one property's loaded/added value.
- `DataResetRow(store, id)` restores every property of one component, including
  nested values. Other components with the same owner or type are unaffected.
- `DataResetBlob(instance)` restores the remaining component rows registered for
  one game object and frees that registration's replacement payload blocks. Other instances
  sharing the resource are unaffected.

The engine keeps component row IDs and the game object's registration handle;
these resets do not require searching the datastore by owner. If the engine adds
independent component rows or multiple registrations to a game object, it resets
those retained IDs/handles as well. Owner IDs alone do not merge registrations.
`DataResetTable` is a bulk maintenance/benchmark utility spanning owners of one
independently registered type, not the normal engine reset scope.

Resets preserve IDs and iterators and do not undo additions or removals. Property
and component resets allocate nothing. They copy defaults into existing rows. Replacement string/container blocks remain
until registration/table reset or destruction. Shared row and registration capacity
is reused until the last registration of that resource is removed. Independently
added default bytes remain until table destruction. The caller's packed
buffer must remain unchanged and alive until its resource handle and every
registration are released; the library never frees it.

Structs have separate name/value arrays; lists preserve order and may mix kinds.
Containers may be empty or nested up to 64 levels per property. Loaded containers
are lightweight buffer views: internal `DataGetStructProperty` and `DataGetListValue`
read children without allocating or expanding the tree. Set replaces the whole
top-level value. Dynamic child names/kinds may vary; declared inline structs
require the exact member names and kinds. `DataPropertyDesc.m_Struct` optionally
points to a reusable `DataStructDesc`. Registration compiles the complete layout
into each containing table's metadata, shared by all its rows. It is not a global
type registry. Query paths can bind members of these fixed layouts.
For decoded input, zero-initialize `DataStruct`/`DataList` and fill their input
arrays and counts. Use the accessors for returned views, whose arrays may be NULL.

A store-wide slot array maps 64-bit index/generation IDs to table/row locations.
Swap removal updates the moved row's slot, preserving other live IDs. IDs belong
to their store and are not serialized. Exhausted generations are retired.
Independently registered tables have unique type hashes. Internal operations that
address a table by type affect only these tables; blob registrations use their
own table batches and allow repeated types. Queries and row-ID operations cover both.

Queries copy owner/tag/property filters and cache matching tables. Required
properties match both name/path and declared leaf kind in the same table; each query
field caches its offset and kind. Structural mutations
update these caches between traversals. Hold `DataStoreLock` during iteration; structural
operations return `DATA_RESULT_LOCKED` without changes until the outermost unlock.
Property writes and resets preserve iterators. Owner filters select contiguous matching runs within a table.
Iterators and reads allocate nothing. `DataIterNext` caches the current table,
row identities, bindings, mutable bytes and stride once per batch. Typed getters
compile to a fixed payload copy and check the requested kind. Row/field advancement
and getters do not read a store revision, lock counter or parent-step counter.
The row cursor borrows batch state and keeps row indices. Field cursors select
shared metadata; numeric pointers use the batch base, row stride and cached offset.
All cursors are stack-owned and allocation-free. Consumers must rebuild against the
updated SDK header. Hash-based
property access still scans shared metadata; there is no direct column view.
Borrowed strings and container views must be copied before a store write, reset or iterator step if longer
retention is needed. Destroy queries before the store and stop using iterators
before destroying their query. The caller synchronizes concurrent access.

The shared layout removes per-row property names and kinds; it does not make this
a Flecs archetype store. Flecs' usual dense tables keep a column per component
identity, with a struct component remaining an array of structs. Our fixed-stride
rows group one data component's fields together. Registrations from the same loaded
resource share dense table storage. Property lookup, generic container decoding and
instantiation still have costs to measure.

## C usage

Typed get/set functions avoid constructing a tagged `DataValue` for scalar and math
properties. `DataGetPropertyVector3` writes a `DataVector3` through its output pointer;
`DataSetPropertyVector3` copies from a `const DataVector3*`. Vector3/Vector4/Matrix4
contain `m_Values[3]`, `[4]` and `[16]`; matrix elements use column-major order.
The other suffixes are `Number` (`double`), `Boolean` (`uint8_t`, zero or one),
`String` (`const char*`), `Vector4` and `Matrix4`. Scalar setters take values directly;
string getters return borrowed bytes, and setters copy the string.

Traversal has three cursors: `DataIterator` selects batches, `DataIterRows` creates
one `DataRowIterator` for the current batch, and `DataRowIterFields` creates a
`DataFieldIterator` for the current row. Advance with `DataIterNext`,
`DataRowIterNext` and `DataFieldIterNext`. Each returns OK or END. Get row identity through `DataRowIterGetId` and
`DataRowIterGetOwnerId`, without supplying an index.

Field iteration visits requested properties, or all top-level properties when none
are requested. Field order is unspecified; inspect names and kinds to select values.
The requested property list is a filter set, not an output-order contract. A table
missing a requested name/type does not match. Read-only `m_Index` identifies the
current iteration position, not a position in the query descriptor.
`DataFieldIterGetType` and `DataFieldIterGetNameHash` read its shared metadata on demand.
Inline paths report the leaf name. Row cursors hold their parent batch and indices;
field cursors hold the batch reference and selected row/field indices directly.
Their inline Next functions perform an end check and index advancement. Getters
resolve the row bytes and shared field binding when accessed. Typed field reads use
inline wrappers that pass the batch, row index and field index to private library
entry points. Callers continue to use `DataFieldIterGet<Type>(&field, &value)`;
functions suffixed `Internal` are implementation linkage, not standalone SDK APIs.
Holding a row index
does not extend a field cursor's lifetime beyond the next parent step. It does not recurse
into containers. Internal cursor members follow `// private` and are omitted from
SDK member documentation. Do not copy active iterators. Finish using children before
advancing their parent, including when it returns END. Keep parents alive at the
same address and the query/store alive throughout traversal. These lifetime rules
are caller obligations; misuse is not detected by iterator get/set/Next.

Use `DataStoreLock(store)` before `DataQueryIter` and `DataStoreUnlock(store)` after
traversal, including early exits. Locks nest and allocate nothing. The lock is a
single-threaded structural guard, not a mutex; thread synchronization remains the
caller's responsibility. Starting query iteration asserts the lock once; row/field steps and getters do not check it.
Existing values can still be written or reset. Destruction, row addition/removal,
table registration/removal and instance addition/removal return LOCKED before any
changes while a lock is held.

For deferred deletion, append `DataId`s to a caller-owned reusable buffer during
traversal and call `DataRemoveRow` for them after the outermost unlock. Queued rows
remain visible until deletion is applied. IDs survive swap-removal of other rows;
row indices do not. Unlock itself does not flush work. No per-row deletion flags or
general command queue are needed.

`DataFieldIterGet<Type>` / `DataFieldIterSet<Type>` take only the field cursor and
output/value, using its selected metadata. All typed access requires the exact declared kind:
there is no conversion, errors leave outputs/stored values unchanged, and resets
restore loaded/added defaults. A `DataQueryProperty` can name an inline member
path, e.g. `{ light_hash, DATA_VALUE_TYPE_VECTOR3, &color_hash, 1 }`. Creation
copies the hashes and binds the full offset once per table; the row loop uses
`DataFieldIterGetVector3` / `DataFieldIterSetVector3`.

`DataFieldIterGetStructPropertyVector3` reads an immediate
Vector3 member of a STRUCT query field, such as `light.color`, directly into a
`DataVector3`. It checks the member metadata for each row because dynamic struct
layouts can differ. Generic `DataValue`, `DataStruct`, `DataList` and their
accessors are internal; no general public container cursor exists yet.

SDK callers receive an engine-owned store. `src/test/test_data_c.c` is compiled and
executed as C, with a C++ harness supplying the store. For a field declared Vector3,
pass a property hash such as `dmHashString64("color")`:

```c
#include <dmsdk/data/data.h>

DataResult SetLightColor(HDataStore store, DataId id, uint64_t color_property, float r, float g, float b)
{
    DataVector3 value = { { r, g, b } };
    return DataSetPropertyVector3(store, id, color_property, &value);
}
```

The existing light templates store color as a DDF list. Converting it to Vector3
requires an explicit producer choice; DDF conversion is not implemented yet.

Queries accept owner IDs, all-required tags and required properties or inline
member paths with exact kinds; empty filters match everything:

```c
#include <dmsdk/data/data.h>

DataResult SetOwnerNumber(HDataStore store, DataOwnerId owner, uint64_t property, double value)
{
    DataQueryProperty field = { property, DATA_VALUE_TYPE_NUMBER };
    DataQueryDesc desc = { &owner, 1, 0, 0, &field, 1 };
    HDataQuery query;
    DataResult result = DataCreateQuery(store, &desc, &query);
    if (result != DATA_RESULT_OK)
        return result;
    DataStoreLock(store);
    DataIterator iterator = DataQueryIter(query);
    while ((result = DataIterNext(&iterator)) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&iterator);
        while ((result = DataRowIterNext(&rows)) == DATA_RESULT_OK)
        {
            DataFieldIterator field = DataRowIterFields(&rows);
            result = DataFieldIterNext(&field);
            if (result == DATA_RESULT_OK)
                result = DataFieldIterSetNumber(&field, value);
            if (result != DATA_RESULT_OK)
                break;
        }
        if (result != DATA_RESULT_END)
            break;
    }
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return result == DATA_RESULT_END ? DATA_RESULT_OK : result;
}
```

This updates rows owned by the supplied owner that declare the requested Number
property. An error stops the update; earlier writes remain applied. Read a Number
with `DataGetPropertyNumber` by row ID or `DataFieldIterGetNumber` during iteration.

For loops that know their properties, call `DataQueryFindField(query, &property)`
once per field after query creation. It identifies the requested full path and
kind, independent of field order. The handle survives empty results, table changes
and repeated traversals; each table caches its own offset under that handle.
Use it for any row of that query with `DataFieldGetNumber`, `DataFieldGetVector3` and the Boolean,
Vector4 and Matrix4 variants. These return borrowed const pointers without copying,
allocation or repeated name/type/lifetime checks. Use the corresponding kind.
An absent/unsupported field returns `UINT32_MAX` at binding; it must not be passed
to a getter. Strings/containers retain copying and ownership-aware accessors.

`DataFieldGetNumberMut` and the corresponding fixed-type functions prepare a
writable view of the existing row, without allocation or copying. Writes preserve
defaults and nested siblings; strings/containers retain ownership-aware setters.
Borrowed pointers must be finished before row/batch advance, unlock, or another
mutating accessor/reset affecting the row/table. Handles last until query destruction.
Reacquire pointers after mutation/reset according to the borrowing contract.
The existing field iterator remains available for inspecting arbitrary properties.

Engine code uses the private header for store/table/row lifecycle and serialization.
`DataTableDesc` supplies metadata and stride. `DataRowDesc` supplies an owner,
values in metadata order, their count and a component name hash. These construction
operations are not part of the SDK.

## Binary format, version 8

The file header is exactly eight bytes: FOURCC `DMDT` and a little-endian uint32
version (`8`). Earlier prototype versions are rejected. The payload is:

| Byte from file start | Value |
| --- | --- |
| 0 | FOURCC `DMDT` |
| 4 | uint32 version |
| 8 | uint32 table count |
| 12 | uint32 shared-string area offset |
| 16 onward | uint32 table offsets, then padding to an eight-byte boundary |
| Each table offset | Table header, tags, shared metadata, component IDs, byte rows, nested payloads |
| Shared-string area offset | Deduplicated NUL-terminated strings from every table |

An empty blob is 16 bytes. Each table has this layout:

| Byte within table | Value |
| --- | --- |
| 0 | uint64 type hash |
| 8 | uint32 tag count |
| 12 | uint32 property count |
| 16 | uint32 row count |
| 20 | uint32 row stride in bytes |
| 24 | uint32 total metadata count, including inline descendants |
| 28 | uint32 reserved, zero |
| 32 onward | uint64 tags[tag count] |
| After tags | Metadata[total metadata count], 32 bytes per entry |
| After metadata | uint64 component name hashes[row count] |
| After component IDs | uint8 row bytes[row count × row stride], then padding to eight-byte boundary |
| After row bytes | Nested containers, in row/property order |

Each metadata entry stores a uint64 name hash, then six uint32 fields: kind,
byte offset, first child index, child count, byte size and reserved zero. Root
entries come first. A declared inline struct's nonzero child index starts its
contiguous member range; descendant ranges follow in construction order. Member
offsets are relative to their containing struct. Non-inline fields have zero child
index/count and their ordinary kind size. Metadata stays in the blob on load.

Fixed-size values and declared structs are inline in each row. String and dynamic
struct/list fields use eight-byte
absolute blob offsets, restricted to uint32 range. This slot size also holds a
runtime reference for decoded and overridden values on 32/64-bit targets. The file
contains no native pointers, and loading performs no pointer fixups. The maximum
blob size is UINT32_MAX bytes. Field offsets must fit the stride; nonempty fields
cannot overlap and property hashes must be unique within their parent layout.
Member alignment is Boolean/Null 1, float vectors/matrices 4, Number/references 8.
Inline structs use the maximum alignment of their members. Struct size and row
stride include trailing padding to that alignment. Producers can use target C
`offsetof`/`sizeof`; registration and loading reject invalid alignment. Runtime
pointer layouts are checked at compile time. Direct pointer access requires
little-endian native representation; copying accessors retain endian conversion.

Dynamic nested containers start with uint32 byte size and uint32 element count. Structs
then have uint64 field hashes, uint32 kinds and uint32 absolute offsets; lists omit
the hashes. Child payloads follow depth-first. Containers and nonempty child
payloads align to eight bytes; sizes include padding and descendants but exclude
strings. Null children use offset zero and have no payload. Nested metadata stays
separate from payloads; dynamic containers may have different child layouts.

Kinds are Number=0, Boolean=1, String=2, Null=3, Struct=4, List=5, Vector3=6,
Vector4=7, Matrix4=8. Numeric components use little-endian IEEE-754. The writer zeros
padding and deduplicates strings by content across all tables and nesting levels,
checking equality after hashing. Strings follow depth-first first-use order,
without padding between them. Runtime owners, row IDs and reset history are omitted;
current values become reset defaults on the next load.

`DataWriteBlob(store, NULL, 0, &size)` determines the size; a second call writes
all tables to caller-owned memory that must not overlap a borrowed input blob.
A producer can use a temporary store containing one prototype's components.
The caller supplies component identities and handles file I/O.

The internal resource/instance lifecycle is:

1. Read the file into your allocation and pass it to `DataLoadBlob`. This validates
   all tables and borrows that exact buffer without copying bytes. Input must be
   eight-byte aligned and may be read-only. No rows are registered yet.
2. `DataAddBlob(store, blob, owner, &instance)` creates one logical game-object
   registration, retains the resource, assigns fresh row IDs and copies values into
   shared dense rows. Use `DataGetComponentId`
   to associate prototype component names with those IDs. Multiple instances,
   including across stores, can share the same blob.
3. `DataResetRow(store, id)` restores one component. `DataResetBlob(instance)`
   restores all remaining components in this game-object registration and frees
   its replacement payloads.
   `DataRemoveBlob(instance)` removes its rows and frees its replacement payloads.
   Registration slots and row capacity are reused. The final removal releases the
   resource pool and its blob reference. Other instances remain unaffected.
4. `DataDestroyBlob(blob)` releases the caller's reference, never its buffer.
   It may precede instance removal. Store destruction removes its registrations.
   Once all references are released, the caller can free the allocation.

The loader validates the directory, table/row bounds, metadata layout and kinds,
recursive containers/depth, unique names, boolean values, string starts/termination
and first-use order. It rejects malformed input and trailing bytes before returning
a handle. Registration and reads do not expand nested values.

The intended `.goc` integration puts all data-component tables in one associated
`.datac`. **Bob still writes individual DDF `.datac` resources.** Aggregation in the
`.goc` builder, DDF conversion, Lua and gamesys adapters remain future work. See
[DATASTORE.md](../docs/DATASTORE.md) for the relationships and layout diagrams.
The API and format are experimental.

## Build and test

From a configured Defold build shell, or with `DEFOLD_SDK_ROOT` pointing to a built
SDK:

```sh
cmake -S engine/data -B engine/data/build/arm64-macos -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build engine/data/build/arm64-macos --target run_test_data
```

The root build includes `data` in the engine library list and supports
`-DDEFOLD_SELECTED_ENGINE_LIBS=data`. Keep standalone and top-level build roots
separate, because the top-level build uses per-library output paths.

Tests cover shared metadata, byte strides, fixed property kinds, C math get/set,
nested views, copied decoded input, reset, bulk insertion atomicity, stale IDs,
live queries, structural locks and deferred removal, golden binary fixtures and malformed buffers,
shared multi-table blobs and independent instance reset/removal. No timing assertions.

## Flecs benchmark

The [mixed-instance benchmark design](BENCHMARKS.md) describes the implemented
suite and remaining variants. The original two-number baseline below remains
available as `benchmark_data`; the new executable is `benchmark_data_mixed`.

Flecs is optional, built from a caller-supplied checkout. It is never downloaded or
linked into the runtime library. Benchmarks are explicit targets, excluded from
normal builds and automated correctness tests.

```sh
cmake -S engine/data -B engine/data/build/arm64-macos \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
  -DDEFOLD_DATA_FLECS_DIR=/path/to/flecs
cmake --build engine/data/build/arm64-macos --target benchmark_data
engine/data/build/arm64-macos/src/test/benchmark_data 100000 7
```

Arguments are row count and sample count (1–31). Output is CSV with median and
minimum nanoseconds per row. One warmup is discarded and library execution order
alternates. Reads repeat ten times. A fixed shuffled permutation drives get/set/
removal. Checksums and row counts are verified, and operation errors are checked.
Fixtures and inputs are built outside timed regions. Both insertion paths include
allocation/growth and return IDs into caller-owned arrays. Query creation is outside
the iteration timing; no queries are registered during insertion. Stores are fresh
for each insertion case.

Both sides store two numeric properties, an owner and a tag. Flecs uses separate
scalar components and its C API (`ecs_insert_w_values`, `ecs_bulk_init`,
`ecs_get_id`, `ecs_set_id`, cached queries and `ecs_delete`). Its benchmark target
defines `FLECS_NO_CPP` and `FLECS_NDEBUG`. Defold uses the normal optimized build
with engine assertions retained. No LTO is used.

Both sides use public query/get/set APIs for value access. Defold copies fixed
rows into mutable storage on creation, retains reset defaults, and allocates
nothing for scalar writes. Binary write/load cases are Defold-only and use the
internal resource API. This small diagnostic has one table and two numbers;
use the mixed suite for the main comparison.

## Mixed-instance benchmark

[Current results, raw samples and run metadata](benchmarks/comparisons/c21-case-queries/RESULTS.md)
cover one million runtime-created instances and one million instances from
resource blobs, with seven samples per backend/configuration. The resource case
uses 16 rows per logical registration and six shared Defold tables. The broader
suite remains implemented; the reduction to eight core workloads in
[BENCHMARKS.md](BENCHMARKS.md) is proposed.

Each C++ case keeps its query definition next to its traversal:
`CreateLightColorQuery` in `benchmark_data_light_color.cpp`,
`CreateHealthPositionQuery` in `benchmark_data_health_position.cpp`, and
`CreateExplosionQuery` in `benchmark_data_explosion.cpp`. The shared runner only
owns setup/teardown and sequencing. Queries and field bindings are created once,
outside timed traversal, and creation is measured separately. Explosion has its
own health/position query with no tag filter; its row loop applies radius 50.
Flecs declares health read/write for Explosion and both fields read-only for the
health/position scan. All backends now retain five dense queries; the resource
fixture retains two.

```sh
cmake --build engine/data/build/arm64-macos --target benchmark_data_mixed
engine/data/build/arm64-macos/src/test/benchmark_data_mixed 1000000 7 all 0 > mixed.csv
engine/data/build/arm64-macos/src/test/benchmark_data_mixed 1000000 7 all 16 > packed.csv
python3 engine/data/src/test/summarize_benchmark.py mixed.csv packed.csv > summary.csv
```

The measured C++ loops are split by workload, with explicit `_Defold` and `_Flecs`
functions. `benchmark_data_mixed.cpp` builds the fixture and runs the suite;
`benchmark_data_common.h/.cpp` share fixture descriptions, timing and validation.

| Workload | Source |
| --- | --- |
| Explosion | [benchmark_data_explosion.cpp](src/test/benchmark_data_explosion.cpp) |
| SpotLight/all-light color sum | [benchmark_data_light_color.cpp](src/test/benchmark_data_light_color.cpp) |
| Health + position, including enemy filtering | [benchmark_data_health_position.cpp](src/test/benchmark_data_health_position.cpp) |
| Shuffled position update | [benchmark_data_shuffled_position.cpp](src/test/benchmark_data_shuffled_position.cpp) |
| Individual/bulk creation | [benchmark_data_create.cpp](src/test/benchmark_data_create.cpp) |
| Dynamic additions | [benchmark_data_add.cpp](src/test/benchmark_data_add.cpp) |
| Removal and replacement | [benchmark_data_churn.cpp](src/test/benchmark_data_churn.cpp) |
| Packed population/reset/unload | [benchmark_data_packed.cpp](src/test/benchmark_data_packed.cpp) |
| Packed scalar reads/updates | [benchmark_data_packed_access.cpp](src/test/benchmark_data_packed_access.cpp) |

Arguments are rows (a multiple of 1,000), samples (1–31), backend (`all`, `data`,
`flecs_rows`, `flecs_columns`), and packed rows per registration (`0`, `1`, `4`,
`16`; default `0` selects decoded tables). In packed mode each type's population
must divide evenly by the group size. One million works for all group sizes.

For CPU profiling, append `spot_color 3000` with a single backend and decoded
tables, e.g. `benchmark_data_mixed 1000000 7 data 0 spot_color 3000`. The fixture
and query are created once; only the existing SpotLight scan repeats 3,000 times
per sample. Attach the profiler after `Profile ready` appears on stderr. Use fewer
passes for timing without a sampler. The memory executable accepts the same mode.

For Explosion, append `explosion 2000`. It repeats the existing radius-50 scan
with health writes, resetting values before every pass. Numeric writes use
existing mutable row storage and allocate nothing. Each pass emits its own result; resets and expected
checksum calculation are outside timing and must also be excluded from sampled
stacks.

The fixture uses 100k SpotLight, 150k PointLight, 10k Player, 240k Enemy, 250k
Pickup and 250k Breakable instances at the default size. It keeps 250k light rows
and 500k health/position rows. Defold declares an inline Light layout and binds
`light.color` once per table; the loop reads it with public `DataFieldGetVector3`.
Flecs uses inline Light structs, either in concrete components or as a separate
component. Traversal uses `ecs_query_iter`, `ecs_query_next`, and
`ecs_field_w_size`, followed by ordinary C member access. Bevy uses
`QueryState::iter` / `iter_mut`, followed by Rust member access. No backend
traverses private ECS storage. Scalar/math widths match. Both Flecs variants include owner and
component identity values. Input generation and serialization are outside timing.

The decoded suite covers individual/bulk creation, live-query insertion of 10%
more rows in batches of 100, nested color sums, typed property queries, one
explosion, one shuffled position update, and 1% removal/replacement. Explosion
scans all health/position candidates and subtracts 25 health (clamped at zero)
within radius 50 of the origin. Shuffled position visits each instance once,
reads its position, increments X by one, and writes it back. Both start from reset
defaults and include first writes; every resulting health/position is validated.
Each case describes the workload, independently of how a backend accesses its
values. Query/read/write workloads use only the public SDK functions, including
`DataFieldGetVector3` for the bound light.color field. Fixture construction and the
separately named engine lifecycle/I/O cases use internal creation/loading APIs.
Access-method comparisons belong in internal profiling and experiments.
Writes use one pass; read cases use five warm passes. Reset, validation and
formatting are outside measurement. Query construction is measured separately.

Packed mode instantiates six prebuilt prototype blobs. Each type's values repeat
per registration on all backends, and each group shares an owner. Population
includes copying mutable rows and collecting runtime IDs. Defold shares blob
metadata/defaults; Flecs and Bevy populate mutable components. The suite measures scalar writes
on 0%, 1% and 10% of shuffled rows, first/repeat updates, full-population random
reads, health queries, Data group reset and unload without live queries. Data
preserves defaults; Flecs ordinary mutation does not implement that contract.

CSV records raw samples, visited/operation counts, hits, batches, total ms,
ns/operation and checksums. Comment lines record fixture parameters. Packed Data
runs also emit `# arena` snapshots with mutable row capacity, replacement-payload
block counts, used bytes (including alignment) and capacity (excluding headers). These
snapshots are collected outside timers, without allocation instrumentation in
hot paths. They do not represent total process memory or total library memory.


## Proposed threaded update benchmark

Planned eighth core workload: **Threaded update with content streaming**. It is
not implemented or included in current comparison results. The
[parallel access design](../docs/DATASTORE.md#proposed-parallel-access) keeps
thread creation and job scheduling in the caller. The Defold benchmark uses
`HJobContext`, the handle exposed by the engine job-system API.

Use a deterministic million-instance population and repeat an update loop.
For this workload, initial and newly loaded health values are in `[0, 100]`,
including values at and just below the cap. The existing single-thread fixture's
`[100, 200]` health distribution is not suitable for capped regeneration.

1. Acquire executions for component updates and split their results into explicit
   contiguous ranges. Submit these ranges as jobs. Keep query creation beside the
   corresponding update function and reuse queries across frames.
2. Run independent updates concurrently, with a fixed dependency chain of Movement,
   then Explosion, then Regenerate. Light contribution runs independently.
   Explosion reads position and reads/writes health; Regenerate requests read/write
   health and applies `min(100, health + 0.25)` to each match. This covers both
   read/write and write/write conflicts. The dependency order makes capped health
   updates deterministic; busy/retry alone would not establish that order.
   Regenerate is part of this eighth workload, not another core benchmark. These
   updates exercise the separation needed for sprite and model jobs without adding
   more fixture types.
3. On the main thread, prepare a fixed stream of blob loads and unload requests
   while jobs run. Reuse prepared input bytes for repeatable timing; this measures
   content instantiation/removal, not filesystem throughput.
4. Process job completions and retry busy executions without blocking workers.
   Once frame updates finish, stop new admissions, enter the mutation phase and
   apply all pending removals/additions. Release resources only after their last
   borrower is gone. Resume queries on the changed population next frame.

Run the same seeded frames with 1, 2, 4 and 8 workers. Verify per-frame component
values, light reductions (with floating-point tolerance), live counts and removed
IDs against a sequential reference. Include growth, swap removal, registration
reuse and final resource unload. Deterministic concurrency tests separately verify
that conflicting executions and structural changes are rejected while access is
held, including nested-member conflicts and cleanup after a job group finishes.
Use race detection when supported; timing thresholds are not correctness tests.

Measure complete frame latency, including dispatch, completion handling and actual
content changes, plus time waiting to apply changes. Report median and p95 latency,
allocation requests, peak additional memory and retained-memory change. Allocation
instrumentation must support concurrent callers. Identify job-system and data-store
memory separately, with both included in the end-to-end totals. Flecs/Bevy versions
must use the same update dependencies, content stream and worker budget through
their supported APIs before presenting this as a cross-library comparison.

### Caller-side job submission proposal

Pseudocode: `DataQueryTryBegin`, `DataQueryExecutionGetCount`,
`DataQueryIterRange`, `DataQueryEnd`, the busy result and the execution type are
proposed APIs. `Update`, `RangeJob`, dependency helpers and buffer preparation
belong to the caller. `JobSystemCreateJob`, `JobSystemPushJob` and
`JobSystemUpdate` are existing engine APIs. Each update has a pre-created query,
a worker callback, and a bound health field where needed. `TryBegin` returning
busy acquires nothing, so the update remains waiting for a later attempt.

`first/count` select a half-open range `[first, first + count)` in the execution's
matched rows. They can split or cross table batches. Chunk size is the caller's
choice and need not equal a thread's share; jobs may outnumber worker threads.

```cpp
// Main thread. Each Update and its range records have stable storage until done.
while (!AllUpdatesDone(updates))
{
    JobSystemUpdate(jobs, 0); // Delivers completion callbacks on this thread.
    PreparePendingContent();  // Reads/prepares blobs; no live-store mutations.

    for (uint32_t u = 0; u < update_count; ++u)
    {
        Update* update = &updates[u];
        if (update->m_State != WAITING || !DependenciesDone(update))
            continue;

        DataResult result = DataQueryTryBegin(update->m_Query, &update->m_Execution);
        if (result == DATA_RESULT_BUSY)
            continue;
        Check(result == DATA_RESULT_OK);

        const uint32_t chunk = 4096;
        uint32_t       count = DataQueryExecutionGetCount(&update->m_Execution);
        uint32_t       job_count = count / chunk + (count % chunk != 0);
        PrepareRangeStorage(update, job_count); // Reusable, never moved in flight.
        update->m_Remaining = job_count;
        update->m_State = RUNNING;

        for (uint32_t j = 0; j < job_count; ++j)
        {
            RangeJob* range = &update->m_Ranges[j];
            range->m_Execution = &update->m_Execution;
            range->m_First = j * chunk;
            range->m_Count = Min(chunk, count - range->m_First);

            Job  job = { update->m_Process, OnRangeFinished, update, range };
            HJob handle = JobSystemCreateJob(jobs, &job);
            Check(handle != 0);
            Check(JobSystemPushJob(jobs, handle) == JOBSYSTEM_RESULT_OK);
        }

        if (job_count == 0)
            FinishUpdate(update); // Release even an empty execution.
    }

    // Other main-thread work or a caller-owned completion wait can go here.
}

ApplyPendingContentChanges(); // All executions have now released their access.
```

The Regenerate worker uses its own iterator; Explosion has a separate callback
with its position test and damage calculation:

```cpp
int32_t RegenerateJob(HJobContext jobs, HJob job, void* context, void* data)
{
    const Update*   update = (const Update*)context;
    const RangeJob* range = (const RangeJob*)data;
    DataIterator    it = DataQueryIterRange(range->m_Execution, range->m_First, range->m_Count);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            double* health = DataFieldGetNumberMut(&rows, update->m_HealthField);
            *health = Min(100.0, *health + 0.25);
        }
    }
    return 0;
}

void OnRangeFinished(HJobContext jobs, HJob job, JobSystemStatus status, void* context, void* data, int32_t result)
{
    Update* update = (Update*)context;
    RecordRangeResult(update, status, result);
    if (--update->m_Remaining == 0)
        FinishUpdate(update);
}

void FinishUpdate(Update* update)
{
    DataQueryEnd(&update->m_Execution);
    update->m_State = DONE;
}
```

Workers never change `m_Remaining` or `m_State`; these fields are confined to
the main thread, so they need no atomics. The last callback releases the
execution. Dependent updates become eligible on the next scheduling pass. Keep
both the execution and all job records alive until then. The sketch shows normal
execution; a real submission failure/cancellation path must also drain already
submitted jobs and release acquired access exactly once before reusing storage.
Failed prerequisites must fail their dependents rather than run partial updates.

## Memory benchmark

`benchmark_data_memory` runs the same mixed workloads and validations with allocation
accounting. It is a separate executable; `benchmark_data_mixed` keeps its normal
allocator and timing output. Both use the same arguments and optional Flecs checkout.

```sh
cmake --build engine/data/build/arm64-macos --target benchmark_data_memory
engine/data/build/arm64-macos/src/test/benchmark_data_memory --self-test
engine/data/build/arm64-macos/src/test/benchmark_data_memory 1000000 7 all > memory.csv
engine/data/build/arm64-macos/src/test/benchmark_data_memory 1000000 7 all 16 > memory-packed.csv
python3 engine/data/src/test/summarize_benchmark.py memory.csv memory-packed.csv > memory-summary.csv
```

[Recorded million-instance memory results](benchmarks/comparisons/c21-case-queries/RESULTS.md#runtime-memory) include
seven samples for decoded tables and packed registrations of 1, 4 and 16 rows.
The memory CSV has no timing columns. Per-operation measurements are:

| Columns | Meaning |
| --- | --- |
| `before_bytes`, `live_bytes`, `peak_bytes` | Requested backend heap bytes at entry, exit and peak during the operation, including capacity and library headers |
| `live_blocks`, `peak_blocks` | Outstanding heap allocations at exit and peak during the operation |
| `allocations`, `reallocations`, `frees` | Allocation, resize and free calls during the operation; sum allocations and reallocations for all allocation requests |
| `allocated_bytes` | Sum of allocation and resize request sizes, including full new sizes for realloc; this is traffic, not retained memory |
| `total_*` | Cumulative calls/requested bytes since this store/world was created, including setup and between-case resets/cleanup |
| `fixture_bytes`, `fixture_blocks` | Separate live benchmark input arrays, output-ID arrays and packed-instance handle arrays |
| `shared_bytes`, `shared_blocks` | Separate live source blob buffers and resource handles, shared by all Data registrations |

Setup, query destruction and final teardown have additional memory records. Peaks
start from the live bytes/blocks at operation entry. No tracked backend allocations
may survive teardown. Fixture/resource allocations must also be released at exit.
The summarizer reports median/min/max for every memory metric and rejects mixed
memory/timing input.

The executable intercepts ordinary C++ `new`/`delete` (including dlib arrays) and
Flecs OS `malloc`/`calloc`/`realloc`/`free`/`strdup` callbacks. It counts requested heap
payloads, including the libraries' own arena/pool headers and unused capacity.
It does not count individual values carved from those blocks as heap allocations.
Allocator rounding, allocator/tracker headers, stack/static storage, platform/libc
internals and RSS are excluded. Reallocation peaks reflect the new requested size,
not an allocator's temporary old/new copy. `realloc(NULL, size)` is a resize request;
`realloc(ptr, 0)` counts a resize request and the resulting free. Null frees do not count.
Tracking is single-threaded and does not cover over-aligned C++ allocation APIs,
which these workloads do not use.

Fixture bytes include generated representations for all three backends, even in a
single-backend run; they are not part of the library comparison. Packed source bytes
are shared Data assets; Flecs copies the equivalent values into mutable component
arrays. Defold additionally retains reset defaults; Flecs restores from fixture data outside the measured store.


## File-to-store allocation budget

The first registration of a resource allocates shared table objects, initial row
storage and a page of registration records together. Later registrations reuse
capacity; dense arrays grow geometrically and registration pages hold up to 256
records. Metadata/defaults and shared payloads remain in the caller-owned file buffer.
Store registries reserve capacity once for the complete blob. Live query caches
can grow separately.

[File-to-store allocation measurements](benchmarks/file-loading.csv) include the
file buffer, validation handle and registration: **5 requests with fresh store
capacity, 3 with reused capacity**, across 1/6/1024 tables and one million rows.
These were rechecked before committing the library. This differs from the
resource-instance benchmark's repeated game-object registrations.

```sh
cmake --build engine/data/build/arm64-macos --target benchmark_data_blob_load
engine/data/build/arm64-macos/src/test/benchmark_data_blob_load --expect-bounded
```

## Bevy comparison

The optional Rust harness in `src/test/bevy` adds **Bevy ECS 0.19.1**, pinned with
`Cargo.lock`. It uses the standalone ECS crate, single-threaded `World` and cached
`QueryState` APIs, normal change tracking, and default table storage. It has no
renderer or scheduler and is not an engine dependency. Bevy is licensed under
MIT/Apache-2.0; the first-party harness uses the Defold license.

Both complete-struct and separate-component variants match the C++ fixture's
seed, type mix, field widths, owner/component identities, update order and query
results. Struct queries use a fixed typed adapter for common fields; they do not
reflectively discover arbitrary same-named properties. Packed-mode values repeat
the same prototypes but are copied into mutable Bevy storage, as in Flecs. Native
shared-blob defaults, reset and grouped unload are not measured for
Bevy. Restoring fixture values between cases is outside measurement.

Build timing and allocation tracking separately, from the repository root:

```sh
cargo build --release --locked --manifest-path engine/data/src/test/bevy/Cargo.toml --target-dir engine/data/build/bevy
cargo build --release --locked --features memory --manifest-path engine/data/src/test/bevy/Cargo.toml --target-dir engine/data/build/bevy-memory
cargo test --release --locked --features memory --manifest-path engine/data/src/test/bevy/Cargo.toml --target-dir engine/data/build/bevy-memory -- --test-threads=1
cmake --build engine/data/build/arm64-macos --target run_test_data benchmark_data_mixed benchmark_data_memory
python3 engine/data/src/test/run_benchmark_comparison.py engine/data/benchmarks/comparisons/my-run \
  --label 'Defold / Flecs / Bevy' --defold-configuration 'Shared mutable tables and public typed field pointers'
python3 engine/data/src/test/build_benchmark_report.py
```

The existing optional Flecs checkout must be configured for the C++ targets.
Cargo downloads its locked dependencies on the first build. The comparison
runner requires a new output directory, defaults to one million rows/seven
samples, and accepts `--rows 16000 --samples 1` for a smoke run. It records all
five backends for runtime-created instances and one packed fixture (16 rows per
registration by default), alternating executable order between layouts. C++ and Rust runs use separate processes and compilers;
the manifest records their versions, flags and source/binary hashes.

The runner rejects differing workload coverage, operation counts, hit counts or
checksums between Bevy and Flecs. Memory mode wraps Rust's global allocator and
records requested payload bytes, allocation/resize/free calls and live blocks.
Fixture allocations and caller ID arrays are attributed separately; tracking
headers, allocator overhead and RSS are excluded. The timing binary has no
allocation tracker. `batches` counts matched Bevy tables per query pass.

[Current comparison tables and caveats](benchmarks/comparisons/c21-case-queries/RESULTS.md)
are also available as Markdown for viewers that display HTML as source.

## Standalone HTML report

Open [`benchmarks/report.html`](benchmarks/report.html) directly in a browser.
It shows only the latest completed Defold/Flecs/Bevy comparison: all performance
bar charts first, then net heap-change and allocation-request charts. Main charts
show three bars: Defold, Flecs and Bevy, using separate components for Flecs and
Bevy. Their complete-struct variants remain in the current detailed tables.

Instances from resource blobs use 16 rows per logical reset/unload group, stored
in six shared Defold runtime tables. Registration size does not limit physical
table capacity. Only that group size is measured in the current comparison.
Runtime-created instances insert values through each library's creation API;
Defold uses one table per type. Fixture preparation and file loading are outside
the timing. Each fixture group has a jump link; viewing the main charts requires
no case selection.

Heap change is retained bytes after minus before each operation, calculated per
sample before summarizing. Detailed tables include before/after totals, peaks and
live blocks. Methodology explains current query setup, storage differences,
validation and memory-accounting limits. Raw CSVs and measurement metadata from
that same run are embedded for offline downloads. The builder selects the latest
completed run and verifies its result hashes. Earlier runs, experiments and profiles
are local artifacts excluded from Git and from the report. New runs remain local
until selected explicitly in `benchmarks/.gitignore`.

Regenerate it from the recorded CSVs and manifests with Python's standard library:

```sh
python3 engine/data/src/test/build_benchmark_report.py
```

The report's Print / PDF button prints all overview charts and the current detail
selection. Expand the full results or measurement metadata before printing to
include them.
