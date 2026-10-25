# Data store

## File Format

A blob contains a header, a directory, component tables and one shared string
area. The eight-byte header is FOURCC **`DFDS`** (Defold DataStore) followed by
uint32 version **1**. The directory locates each table and the string area.

Each table stores shared field metadata, a **row-record array**, and a separate
**component value array**. Metadata holds field-name hashes, kinds and byte offsets;
value rows hold values and alignment padding. Record `i` describes value row `i`.
Strings and dynamic containers use blob-relative offsets. Serialization saves
current values as the next load's defaults.

`DSRow` has the same 32-byte layout on disk and at runtime. Its `m_Group` field
is present in the file as **zero**, along with zeroed runtime slot and source
indices; the component name hash is preserved. Instantiation bulk-copies each
table's record array and value array separately, then fills the records' group,
stable-ID slot, source row and registration index. Groups are caller-provided owner
handles; the caller associates them with game objects or other owners.
No live handles or pointers are written to disk.

![Generic blob layout and one expanded table. Solid arrows follow blob offsets;
dashed arrows expand the table, row record, field metadata and dynamic container.](images/datastore-binary.svg)

The fixed layouts shown in the schematic are declared in the private
[data.h](../datastore/src/data.h).

### Value storage

Align each member to its type and an inline struct to its most-aligned member.
Include trailing padding in the row stride. The producer uses target-compatible
`offsetof`/`sizeof` or equivalent offsets and sizes.

| Value | Stored bytes | Alignment |
| --- | --- | --- |
| Number | 8 (`double`) | 8 |
| Float / UInt32 | 4 (`float` / `uint32_t`) | 4 |
| Boolean | 1 | 1 |
| Vector3 / Vector4 / Matrix4 | 12 / 16 / 64 (`float32` components) | 4 |
| String / dynamic struct / list reference | 8 (blob offset) | 8 |
| Declared inline struct | Its members and padding | Maximum member alignment |

Matrices are column-major. Blob buffers and the start of each table's row array
must be eight-byte aligned. Subsequent rows follow the row stride and their
members' alignment requirements. Loading validates layouts, alignment and version.
Scalar/math fields permit native typed pointer access on the engine's little-endian
targets. Set changes a value, never its declared kind.

**Matching a C/C++ struct:** member offsets and row size match exactly when the
metadata comes from that native struct: use `offsetof(Type, member)` for
`DSFieldDesc.m_Offset`, `sizeof(NestedType)` for `DSStructDesc.m_Size`, and
`sizeof(RowType)` for `DSTableDesc.m_RowStride`. Use plain structs and the
corresponding SDK types: `double` for Number, `float` for Float, `uint32_t` for
UInt32, `uint8_t` for Boolean, and `DSVector3`/`DSVector4`/`DSMatrix4` for
math values. Registration preserves those offsets and sizes after validation.

The file records this layout; loading does not compare it with a separately
declared C/C++ struct or rearrange it for another ABI. A producer and consumer
must agree on types, offsets and stride. Custom packing or SIMD types do not
automatically match: `DSVector3` is 12 bytes with four-byte alignment, and
stronger alignment such as 16 bytes is not guaranteed by the format. The Light
example below shows how matching value rows can be read as native structs.

The recursive kinds in [data_ddf.proto](../gamesys/proto/gamesys/data_ddf.proto)
remain supported, including null. Declared structs live inline. Lists and structs
without a fixed layout use dynamic payloads: `DSFileContainerHeader`, member
name hashes (structs only), child kinds, blob offsets and child payloads. Children
can differ between rows, with at most 64 container levels including enclosing
inline structs. Equal strings share bytes in the final string area. Accessors
resolve string/container offsets; reference slots are not native pointers.
Consequently, a blob row containing references cannot be read as a C struct
containing pointers; `DSReference` is a native input descriptor, and stored
references are read through the typed API.

## Example: PointLight and SpotLight

```cpp
#include <dmsdk/datastore/datastore.h>

struct Light
{
    DSVector3 color;     // Byte 0; four padding bytes follow.
    double      intensity; // Byte 16.
}; // 24 bytes

struct PointLight
{
    Light  light; // Byte 0.
    double range; // Byte 24.
}; // 32 bytes

struct SpotLight
{
    Light  light;            // Byte 0.
    double range;            // Byte 24.
    double inner_cone_angle; // Byte 32.
    double outer_cone_angle; // Byte 40.
}; // 48 bytes
```

![Separate PointLight and SpotLight row example, drawn at the same byte scale.
Both embed the Light fields at the same offsets. Padding and template values
are shown explicitly.](images/datastore-composition.svg)

Both types embed a **`Light light`** member containing `color` and `intensity`.
PointLight adds `range`; SpotLight also adds `inner_cone_angle` and
`outer_cone_angle`. Range and cone angles use Number (`double`). Light occupies
24 bytes inside each row, with no separate pointer, row ID or lifetime. PointLight
rows are 32 bytes; SpotLight rows are 48.

Here `pointlight_rows` and `spotlight_rows` point to the respective value arrays
in a validated blob whose types, offsets, stride and alignment match these structs.
They point to the first value row, not the file header.

```cpp
const PointLight* point_lights = (const PointLight*)pointlight_rows;
const SpotLight*  spot_lights = (const SpotLight*)spotlight_rows;

DSVector3       color = point_lights[0].light.color;
double            intensity = spot_lights[0].light.intensity;
```

This illustrates the file's value layout. Runtime queries expose typed field
pointers through the public iterator API.

For example, the `intensity` field description uses `offsetof(Light, intensity)`;
the inline Light description uses `sizeof(Light)`; the PointLight table uses
`sizeof(PointLight)` as its row stride. The numbers in the schematic are the
resulting layout, not offsets to hard-code in registration.

The values come from the existing [point-light](../../editor/resources/templates/template.point_light)
and [spot-light](../../editor/resources/templates/template.spot_light) templates.
Those templates are currently flat and represent color as a list. This example
shows the intended composition and conversion to Vector3; the producer
conversion is not implemented yet.

A producer can reuse one `DSStructDesc` for Light. Each containing table stores
its own metadata, shared by all its rows. For these layouts, the metadata for
`light.color` contains its full-name hash, `DS_TYPE_VECTOR3` and byte offset 0;
`light.intensity` has `DS_TYPE_NUMBER` and byte offset 16. Another containing
type can place Light at a different offset.

The producer supplies local text names in `DSFieldDesc::m_Name`. Registration
hashes complete names such as `light.color` and stores absolute row offsets;
text names are not retained. Registration and loading reject duplicate full-name
hashes. Embedding Light does not make SpotLight a standalone Light component or
assign it a tag. There is no full schema system beyond these layouts.

## Examples: light lists and references

```cpp
struct LightRig
{
    DSReference lights; // List input; eight-byte reference slot.
};

struct ReferencedPointLight
{
    DSReference light; // Dynamic Light input; eight-byte reference slot.
    double        range;
};
```

These native input structs reuse the `Light` definition above. `LightRig` holds
a variable-length list; `ReferencedPointLight` holds one Light outside its own
row. `DSReference` supplies pointers during creation. In the file, each slot
contains a **uint64 blob offset**, and the referenced payload is in the same blob.
Neither row contains a native `Light*` or `Light[]` when stored.

Both creation examples use this native Light layout. The functions each register
their example table once and add one row to an existing store.

```cpp
#include <stddef.h>
#include <dmsdk/dlib/hash.h>

static const DSFieldDesc g_LightFields[] = {
    { .m_Field = dmHashString64("color"), .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(Light, color) },
    { .m_Field = dmHashString64("intensity"), .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(Light, intensity) }
};

static const DSStructDesc g_LightLayout = {
    .m_Fields = g_LightFields,
    .m_FieldCount = 2,
    .m_Size = sizeof(Light)
};
```

### A struct containing a list of lights

![LightRig's eight-byte value row points to a list container holding two Light
structs. Typed fields, local byte offsets, padding and blob-offset references are
shown for the list and each Light.](images/datastore-light-list.svg)

```cpp
DSResult CreateLightRigExample(HDSContext store, DSGroupId group, DSId* out_id)
{
    DSFieldDesc field = {
        .m_Field = dmHashString64("lights"),
        .m_Type = DS_TYPE_LIST,
        .m_Offset = offsetof(LightRig, lights)
    };
    DSTableDesc table = {
        .m_Type = dmHashString64("LightRig"),
        .m_Fields = &field,
        .m_FieldCount = 1,
        .m_RowStride = sizeof(LightRig)
    };
    DSResult result = DSRegisterTable(store, &table);
    if (result != DS_RESULT_OK)
        return result;

    Light lights[] = {
        { .color = { { 1.0f, 0.8f, 0.6f } }, .intensity = 1.0 },
        { .color = { { 0.6f, 0.8f, 1.0f } }, .intensity = 2.0 }
    };
    DSStructInput inputs[] = {
        { .m_Layout = &g_LightLayout, .m_Values = &lights[0] },
        { .m_Layout = &g_LightLayout, .m_Values = &lights[1] }
    };
    DSReference elements[] = {
        { .m_Struct = &inputs[0] },
        { .m_Struct = &inputs[1] }
    };
    DSListInput list = {
        .m_Type = DS_TYPE_STRUCT,
        .m_Count = 2,
        .m_Values = elements
    };
    LightRig row = { .lights = { .m_List = &list } };
    return DSCreateRows(store, table.m_Type, group, 1, &row, out_id);
}
```

For STRUCT elements, `DSListInput.m_Values` points to `DSReference[]`, each
describing a native struct through `DSStructInput`. It does not point directly
to `Light[]`. This example gives every element the same layout; the API also
supports different struct layouts and mixed element kinds.

The list payload contains a header, one kind and one offset per element, then
the element payloads. Each Light element is a dynamic struct container with its
own member hashes, kinds and offsets. These elements are not fixed-stride Light
rows, even though the input structs are. They remain packed containers at runtime.

### A referenced Light

![ReferencedPointLight's sixteen-byte value row holds a Light reference and
an inline range. The reference points to a dynamic struct container containing
member hashes, kinds, offsets, color and intensity.](images/datastore-light-reference.svg)

```cpp
DSResult CreateReferencedLightExample(HDSContext store, DSGroupId group, DSId* out_id)
{
    DSFieldDesc fields[] = {
        { .m_Field = dmHashString64("light"), .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(ReferencedPointLight, light), .m_Struct = 0 },
        { .m_Field = dmHashString64("range"), .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(ReferencedPointLight, range) }
    };
    DSTableDesc table = {
        .m_Type = dmHashString64("ReferencedPointLight"),
        .m_Fields = fields,
        .m_FieldCount = 2,
        .m_RowStride = sizeof(ReferencedPointLight)
    };
    DSResult result = DSRegisterTable(store, &table);
    if (result != DS_RESULT_OK)
        return result;

    Light                light = { .color = { { 1.0f, 1.0f, 1.0f } }, .intensity = 1.0 };
    DSStructInput      input = { .m_Layout = &g_LightLayout, .m_Values = &light };
    ReferencedPointLight row = {
        .light = { .m_Struct = &input },
        .range = 10.0
    };
    return DSCreateRows(store, table.m_Type, group, 1, &row, out_id);
}
```

`DS_TYPE_STRUCT` with `m_Struct = 0` declares a dynamic reference. Setting
`m_Struct = &g_LightLayout` would instead declare inline storage and require an
actual `Light` member, as in the earlier PointLight example.

On disk, the referenced Light uses a dynamic struct container. A loaded instance
initially reads that container from the immutable blob. Native creation, as above,
stores the Light in an owned child-table row; replacing a loaded struct does the
same. Child rows can share layout metadata, but each child value belongs to one
parent. They have no public `DSId` and are not independently queried components.

**Ownership:** both functions copy all input values before returning. Their stack
arrays, descriptors and Light values may go out of scope. Passing the same input
to two parents creates independent copies, not shared mutable references. The
parent owns its copied data. Reading uses `DSFieldGetView`, then
`DSViewGetElement` for a list and `DSViewGetField` for a struct member; typed
view getters retrieve the values. These dynamic members are not fixed query
fields such as the inline `light.color`.

## Runtime Format

Include `<dmsdk/datastore/datastore.h>`. The engine registers its `HDSContext` under
`DATASTORE_CONTEXT_NAME` in the context registry. Extensions borrow it with
`ContextRegistryGet(ExtensionParamsGetContextRegistry(params), DATASTORE_CONTEXT_NAME)`.
It remains available through extension app finalization. Finish jobs and remove
your queries and tables during cleanup; only the engine destroys this store.
Standalone tools and tests can own a separate store with `DSCreateStore(0)`.
To tune allocation, pass `DSCreateParams`:

```cpp
DSCreateParams params = {
    .m_PageSize = 512 * 1024,
    .m_RowChunkSize = 256 * 1024,
    .m_MaxSparePages = 16
};
HDSContext store = DSCreateStore(&params);
```

These are the default settings for a null parameters pointer. Zero size fields
select the default sizes; `m_MaxSparePages = 0` releases all empty pages.
Backing pages are allocated lazily and shared by table value chunks. Each chunk
contains a power-of-two number of native rows and, when retained, a parallel
region of defaults. Oversized rows use dedicated allocations. Chunk size is a
target including both regions; unused tail bytes are included in memory reports.

A table's chunks form a dense directory. Swap-removal keeps all but the last
chunk full, so insertion already knows the chunk and free-row count without a
search. Empty chunks return to their page; available pages use an index-linked
list and an exact free-chunk count. Fully empty pages are reused up to the spare
limit, then freed. This trades retained memory for fewer allocations during
repeated spawn/despawn waves.

Queries return one contiguous span within a chunk. The hot row loop still uses
`base + row * stride + field offset`; random ID access resolves one extra chunk
pointer. Growth does not copy existing component values. Row identities, ID
slots, dynamic child tables and small metadata arrays still grow geometrically.
The file format remains linear and is independent of runtime chunk sizes.

OS page size describes virtual-memory management, not the best query batch size.
These backing allocations use the normal allocator, without OS page alignment or
virtual-memory decommit calls. A row chunk may span several OS pages. Choose its
size from scan, lookup, allocation and slack measurements, rather than making it
machine-dependent.

A **datastore** holds runtime tables, stable row IDs, resource registrations and
queries. A **table** groups rows with the same type, tags and field layout.
Each data component occupies one row. A game object can have multiple data
components of different types, or several of the same type; they can share a
caller-provided `DSGroupId`. A **group** owns its rows across tables.
Use the game-object handle directly, or choose an ID for a spawn wave or other owner;
the datastore has no game-object dependency. Different groups share the same type
table. Each row has a runtime `DSId`; loaded components also retain their
prototype-local component name hash.

`DSRemoveRow` removes one row; `DSRemoveGroup` removes all of the group's rows,
owned payloads and blob registrations, including empty registrations. Independent
table registrations remain available. Both operations require all query
reservations to finish. The caller applies queued removals after joining jobs.
Bookkeeping is created when a nonempty row batch or a blob instance first uses an ID.
An individually emptied group keeps its bookkeeping until `DSRemoveGroup`.
The caller controls ID uniqueness and reuse: existing query filters match rows
added later with the same ID. Group `0` denotes ungrouped, caller-managed rows.

```cpp
DSGroupId group = gameobject; // Existing uint64_t owner handle.
DSResult result = DSCreateRows(store, dmHashString64("Enemy"), group, count, enemies, ids);
if (result != DS_RESULT_OK)
    return result;

// Later, at a structural safe point:
DSRemoveRow(store, ids[0]);    // Remove one enemy; the group stays alive.
DSRemoveGroup(store, group);  // Remove the remaining enemies and group bookkeeping.
```

Caller IDs are looked up once per native batch in the group registry.
Group membership uses integer links in the stable row-ID slots. Table growth and
swap removal leave those links valid; group removal visits only its members.
The registry and membership links are runtime bookkeeping, separate from value
rows and query iteration.

`DSTable::m_Rows` uses the file's `DSRow` layout, while `m_Chunks` holds the
separate dense component bytes. Borrowed external tables keep their existing
contiguous value storage. Groups and bookkeeping do not change a component's
native struct layout or row stride. `DSTable` holds runtime storage and ownership,
separate from the file's `DSTableHeader`. `DSValue`, `DSStruct` and `DSList`
are temporary input or borrowed views; they are not serialized as C++ structs.
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

For construction without a blob, public `DSRegisterTable` copies a native C
layout and its tags. `DSCreateRows(store, type, group, count, rows, out_ids)`
copies complete native rows, including inline members, using the registered row
size. One group is assigned to every row; each receives its own ID. Native rows
have no component name. The caller prepares defaults and per-instance values.
Nonzero groups acquire bookkeeping on first use. By default, rows store
only current values: AoS creation writes directly into current storage and reset
returns `DS_RESULT_INVALID_ARGUMENT`. Set
`DSTableDesc.m_ResetPolicy = DS_RESET_RETAIN` to keep an extra copy of supplied
values for reset. With history enabled, swap removal moves current
and default rows together and reuses the removed default slot. Inputs live only
through the call. Native reference fields use
`DSReference`: a string pointer, `DSStructInput` (member layout plus native
bytes), or `DSListInput` (typed array or mixed payloads). These eight-byte
input slots work inside inline structs too. Creation measures payloads for the
whole batch, reserves arena storage for strings/lists, and copies dynamic struct
fields into owned child tables. It does not retain input pointers or deduplicate
equal strings. Replacement, reset and removal recycle string/list payload chunks.
Empty blocks are freed, retaining at most one small spare per arena; partially used
blocks keep stable addresses. Fixed row capacity remains available for later insertions.
List inputs can use one shared element kind and a contiguous native array, or
per-element `m_Types` with `m_Values` holding payload pointers. Mixed lists support
all recursive DDF kinds and the runtime math types; both forms store a kind per
element. Struct elements can each describe different members. `DSValue` and
serialization stay internal.

`DSCreateRowsSoA(store, type, group, count, field_count, fields, out_ids)`
accepts separate native field arrays. Each `DSFieldArray` pairs a root field
hash with `count` values; supply every root field once in any order. Inline
structs use arrays of complete structs. Field names resolve once per batch, then
values copy into complete native rows, retaining reset rows only when requested
by the table policy.
Types and sizes come from the registered layout. Ownership, reset and locking
rules are the same as for `DSCreateRows`.

### Borrowed native rows

`DSRegisterTableView` exposes caller-owned AoS memory through the same queries,
field bindings and typed getters. It copies metadata and optional source-row
indices, but neither values nor reset defaults. Consecutive source indices form
cached query batches; the field getter keeps its normal offset/stride calculation.
Views support scalar and math fields. Unregistering releases the view's IDs and
metadata; the caller retains the values. Create/remove/reset individual rows and
serialization are unsupported for views.

The caller keeps external memory stable and prevents engine access until jobs
finish. Datastore reservations do not lock external owners. Views selecting the
same source rows participate in field-access conflict checks; disjoint selections
can run together. Multiple rows may share read access, but concurrently writable
jobs must select distinct physical rows, including within a single reservation.

### Reading dynamic containers

`DSFieldGetView` reads a row field by ID; `DSRowIterGetView` reads a bound query
field. `DSValueView` is a borrowed, allocation-free view with a private layout.
Use `DSViewGetField` with a local member hash, or `DSViewGetElement` with a
list index; then inspect its kind and use a typed getter such as
`DSViewGetNumber` or `DSViewGetString`. Repeat those steps for nested content.
This supports native, inline and packed values without making child rows entities
or exposing general field enumeration. Finish using views before mutation or query
release.

### Owned dynamic structs

A dynamic struct field is an eight-byte **child-table index and row index**.
Within a component table, objects with the same member hashes and kinds share
one `DSStructTable`, regardless of input member order. Its metadata supplies
C-aligned offsets; its fixed-stride rows contain only values. Nested struct
members use the same representation recursively. These are private storage
tables, with no public `DSId`, tags or separate query membership. Inline
structs such as `Light` above remain in the component row.

![An Enemy row owns a dynamic attributes row. Two attributes rows share metadata
but have separate values. Freed child slots are reused without moving another
parent's child.](images/datastore-owned-structs.svg)

Each child row belongs exclusively to one parent. Reusing an input pointer makes
independent copies, not shared mutable references. Reset keeps the original tree
and releases any replacement tree; removing the parent releases both. Free slots
are linked through their unused row bytes and reused by later insertions. Child
rows do not move, so removal needs no global ID lookup or reference fixups.
Removing the last child row frees its value buffer, including any free slots.
Shared layouts remain allocated until the component table is freed.
Strings and lists inside child rows still use the containing table/registration's
payload arena and follow its reclamation rules.

This applies to owned dynamic struct **fields**. Mixed lists, including their
struct elements, keep the packed container representation. Loaded dynamic
structs continue to reference immutable blob payloads directly; an explicitly
replaced struct uses child rows, and reset returns to the blob. Serialization
writes current child values into the existing version-1 container format. There
is no change to the zero-copy blob loading path.

For scattered reads, public `DSFieldGet*Batch` functions accept an ID array and
one field hash. They stage addresses before copying values, with no allocation.
The count follows the store argument, and output values follow input order. One
`DSResult` reports success or the first error. Outputs may be partially written
on failure; consume them only after `DS_RESULT_OK`. The caller provides stable
storage and synchronization, just as for scalar getters. The library chooses its
internal batch size.

The resource system owns the immutable blob allocation. `DSLoadBlob` validates
and borrows it without copying or changing the bytes; loading alone adds no rows
to a datastore. A store retains references to loaded blobs through its resource
pools. Registrations of the **same loaded blob** append mutable rows to the same
runtime tables and reuse query bindings. Different loaded blob handles remain
separate, even if their layouts match.

![The datastore references caller-owned blobs. Each resource pool has shared
runtime tables and registration records connecting game objects to their rows.](images/datastore-tables.svg)

### Instantiating and removing grouped blobs

`DSAddBlob(store, blob, group, &instance)` instantiates all component rows from
the blob in one operation. For each table it bulk-copies the row-record array and
the fixed-size value array into the resource's shared dense storage, then fills
the records' runtime fields. Value copies stop at chunk boundaries; metadata and
row layout are unchanged. Each table appends to its own storage.
Metadata and defaults remain in the blob; strings and dynamic containers initially
reference its payloads. Pooled
registration records and membership indices track the new rows. Capacity grows
as needed and is reused by later registrations.
Removing its group also removes the registration and invalidates its instance
handle. The caller retains ownership of the original blob and its backing bytes.
Each runtime table records whether its rows contain references. Numeric-only
instantiation and reset copy the bytes directly without walking field metadata.

![Load a resource once, then register game objects A and B. Each registration
appends a point-light row and a spot-light row to the existing runtime tables.](images/datastore-runtime.svg)

Numeric and math writes modify these mutable rows directly, without first-write
allocation or a second private copy of defaults. Replacement strings/containers
belong to the registration. Runtime reference slots distinguish blob offsets,
owned string/list pointers and owned child-row indices; file bytes remain unchanged. Rows added without a blob
store only current values unless registered with `DS_RESET_RETAIN`.

Reset restores values from the shared resource or explicitly retained native
defaults: `DSResetRow` targets one component,
`DSResetBlob` targets one registration's remaining rows, and field reset
targets one value. IDs survive reset; added/removed rows are not undone. Resetting
one game object's registration does not affect another. Component/field reset
releases replacement child rows and string/list payload chunks; complete
registration reset also frees its spare payload block.

`DSRemoveBlob` removes the registration's remaining rows, invalidates their IDs
and frees replacement payloads. Dense removal can move another row; its ID remains
stable. Shared capacity remains until the resource pool's last registration is
removed. The caller may free the blob buffer only after all registrations and the
loaded blob reference have been released.

Optional write protection belongs to the resource owner: a dedicated, page-aligned
mapping could be made read-only with `mprotect` / `VirtualProtect`. This is not
implemented by `DSLoadBlob`; it must not change protection on arbitrary caller
allocations. Mutable runtime rows remain in separate writable storage.

**Engine integration:** the intended `.goc` packaging is one associated `.datac`
blob containing all of a prototype's data-component tables, loaded once and
instantiated for each game object. Grouped blobs and the runtime operations above
exist in `engine/datastore`. Bob still writes individual DDF `.datac` resources;
aggregation and gamesys resource/game-object integration remain future work.

## Queries

A query describes required table tags and full field-name hashes with exact kinds, plus
optional group filters. All requested fields must belong to the same table;
queries do not join separate component rows. For example, require the `light` tag
and `light.color` as Vector3 to visit both light types above, provided their tables
were assigned that tag. Value tests, such as a radius check, run in the row loop.

```c
DSQueryDesc desc = {
    .m_Items = {
        DSTag("light"),
        DSField("position", DS_TYPE_VECTOR3, 0),
        DSField("light.color", DS_TYPE_VECTOR3, 0)
    }
};
HDSQuery query;
if (DSCreateQuery(store, &desc, &query) == DS_RESULT_OK)
{
    uint32_t color_field = DSQueryFindField(query, &desc.m_Items[2]);
    // Reuse query and color_field when iterating.
}
```

`DSTag` and `DSField` hash their strings immediately; they retain no string pointers.
Pass `DS_FIELD_MUTABLE` for writes, or `0` for reads. A descriptor holds up to 64
mixed items, in any order. Zero-initialize unused entries; the first unused item
ends the list, so no count is needed. Each item's kind distinguishes a tag or field
from an unused entry, allowing zero hashes. Group filters use a pointer and count,
allowing group `0` to select ungrouped rows.

![Create a light-color query, cache a binding for each matching table, then reserve
access and iterate batches and rows using a stable field handle.](images/datastore-queries.svg)

1. **Create once:** `DSCreateQuery` copies the populated filters in a `DSQueryDesc`.
   `DSField("light.color", DS_TYPE_VECTOR3, 0)` describes a nested color read.
   No path array or runtime string parsing is needed.
2. **Bind once:** `DSQueryFindField` returns a handle for that full-name hash and
   kind. Each matching table caches its own byte offset under the handle. Missing
   or incompatible fields exclude a table. Adding/removing tables updates
   matches without invalidating the handle.
3. **Reserve access:** `DSQueryTryBegin` returns OK when the requested access is
   available, or BUSY so the caller can retry. Query creation itself reserves nothing.
4. **Iterate:** `DSQueryGetRowCount` counts reserved matching rows.
   `DSQueryIterRange(query, first, count)` creates a cursor over a selected range;
   `DSIterNext` returns `true` for each batch and `false` when exhausted.
   `DSIterRows` creates a row cursor; `while (DSRowIterNext(&rows))` traverses its rows.
   `DSRowIterGetVector3(&rows, color_field)` returns the current color.
5. **Release:** after all iterators and borrowed pointers finish, call
   `DSQueryEnd`. Reuse the query on the next update; destroy it when no longer needed.

A field handle is neither a byte offset nor an index in the query descriptor.
Each query match stores uint32 byte offsets alongside its bindings. The iterator
caches the table's row stride. Typed pointer getters calculate
`batch base + row × stride + field offset` inline, without copying, allocation,
hash lookup or repeated kind checks.
Writable getters such as `DSRowIterGetNumberMut` require a field declared with
`DS_FIELD_MUTABLE`; read-only access is the default. Number, Float, UInt32, Boolean,
Vector3, Vector4 and Matrix4 have typed pointer accessors.

When the C layout is known, `DSRowIterGetData(&rows)` returns the row start as
`const void*`; `DSRowIterGetDataMut(&rows)` returns `void*` for fixed-size writes.
Declare whole-row access when creating the query:

```cpp
DSQueryDesc desc = {
    .m_Flags = DS_ROW_MUTABLE,
    .m_Items = { DSTag("enemy") }
};
```

With the default flags `0`, both row-pointer getters return `0`; access uses
the query's declared fields instead. Field reads are implicit; field writes
require `DS_FIELD_MUTABLE`. Both row-pointer getters require `DS_ROW_MUTABLE`.
Tags and fields filter matching tables; they do not restrict whole-row
permissions to the listed fields.
For example, `Enemy* enemy = (Enemy*)DSRowIterGetDataMut(&rows)` can access the
registered `Enemy` members directly. Check `DSIterGetType` once per batch when
the query matches different layouts. The struct must have the registered size
and member offsets; reference slots remain internal handles, not C pointers.
Whole-row mutable access conflicts with any reader or writer of overlapping
row values. Conflicts are checked at
reservation time. Row getters check flags cached in the iterator, without
accessing the query or store. Never overwrite reference slots. Field-only queries
retain finer-grained concurrency through their typed getters.

Dynamic container members cannot bind because their layout may differ per row.
ID-based access uses typed functions such as `DSFieldGetVector3` and
`DSSetFieldVector3`, using the same full-name hash for inline members.
Generic `DSValue` is internal.

Cursors allocate nothing and borrow their parent's current batch/row. Do not copy
active cursors; finish using children and pointers before stepping a parent,
including when exhausted. Parents must remain alive at the same address. There are no
per-row lifetime checks. Field handles last until query destruction; borrowed
value pointers must also be reacquired after mutations affecting their storage.

### Registering query callbacks

[scheduler.h](../datastore/src/dmsdk/datastore/scheduler.h) registers a query,
processing function and user context under a name in the existing `HDSContext`.
The store copies the descriptor and dependency hashes; queries and user contexts
remain caller-owned. Hash names once during setup. Registration stores no strings;
Defold reverse hashing can supply names for diagnostics. Unregister callbacks
between schedules before releasing their queries, or finish callbacks, end the
schedule and destroy queries at teardown.
Destroying the store frees remaining registrations without invoking callbacks.

```cpp
DSSystemDesc desc = {
    .m_Name = dmHashString64("game.boids.steer"),
    .m_DependsOn = { dmHashString64("game.boids.cells") },
    .m_Query = boids_query,
    .m_Process = SteerBoids,
    .m_Context = boids_context,
    .m_RowsPerJob = 4096,
};
DSResult result = DSRegisterFunction(store, &desc);
```

Between schedules, remove the registration by name:

```cpp
DSResult result = DSUnregisterFunction(store, dmHashString64("game.boids.steer"));
```

This returns `DS_RESULT_NOT_FOUND` if the name is not registered. The query and
user context remain caller-owned.

The callback signature is `void SteerBoids(HDSQuery query, uint32_t first,
uint32_t num_rows, void* context)`. The scheduler reserves the query and submits
one job for the whole query by default (`m_RowsPerJob = 0`). Set `m_RowsPerJob`
to split the query into disjoint ranges with at most that many rows per job.
For 8,500 matches and 4,096 rows per job, the scheduler calls the function with
`(first, num_rows)` values `(0, 4096)`, `(4096, 4096)` and `(8192, 308)`.
An empty query still invokes one job with `(0, 0)`.

Each callback uses `DSQueryIterRange(query, first, num_rows)`, not `DSQueryIter`
over the whole query. `first` is an index in the reserved results, not a row ID;
a range can cross tables. Jobs may run and finish in any order. Returning finishes
only that range; do not leave child jobs running or retain iterators/pointers.
The scheduler keeps one query reservation until **every range finishes**, then
releases it and submits newly unblocked functions. The job system reuses its job
storage; the datastore allocates no per-range descriptors.

Opt in only when ranges can run independently. All jobs receive the same user
context, so use disjoint output slots or synchronize shared writes. Aliased
external data and shared references still need synchronization. For Boids, use
separate callbacks for snapshot collection, cell construction and steering:
`snapshot → cells → steer`. Split the snapshot and steering callbacks; leave
cell construction as one job. Each dependency waits for every range of its
prerequisite, so steering sees a complete snapshot and spatial index.
Keep the population and snapshot storage stable across these stages; apply
population changes between schedules.

| Operation | Responsibility |
| --- | --- |
| `DSBeginSchedule(store, jobs)` | Borrow the caller's `HJobContext`, resolve dependencies, reset invocation state and submit ready functions. |
| `DSSchedulerEvent(store, name)` | Signal the start of a stage and submit functions whose event/dependency/access requirements are satisfied. `BUSY` leaves the event unsignaled until its own prerequisites finish. |
| `DSSchedulerUpdate(store)` | Pump job completion and retry access blocked by external reservations. Returns `OK` when all functions finished; `BUSY` while any await jobs, events or access. |
| `DSEndSchedule(store)` | End the invocation. Returns `BUSY` while jobs run or ready functions await query access; otherwise discards unsignaled events and functions waiting on them. |

Use one main thread for these operations and for pumping the borrowed job context.
The datastore creates no threads and never destroys the context. With zero workers,
`DSSchedulerUpdate` executes at most one queued job on the calling thread. With
workers, jobs run there. Pumping the borrowed context with `JobSystemUpdate` also
delivers completion callbacks, which automatically submit ready dependents.
`DSSchedulerUpdate` may service other users of that shared context; do not call it
recursively from a job or completion callback.

Read/read access and disjoint writes may overlap. Overlapping read/write or
write/write access waits through the query reservation checker, including
external reservations. `m_DependsOn` imposes completion order even on read-only
queries; access conflicts alone do not promise an order. Dependency indices are
cached until registration changes; query conflicts follow current table matches.
Every named prerequisite must be registered as a function or event.

### Engine events

`engine.datastore` defaults to `1`. Set it to `0` in `game.project` to disable
the engine-owned datastore, its worker threads and per-frame scheduling:

```ini
[engine]
datastore = 0
```

When disabled, looking up `DATASTORE_CONTEXT_NAME` returns null. Extensions that
use the engine store must handle its absence. Standalone stores created with
`DSCreateStore` are unaffected.

The engine-owned `DATASTORE_CONTEXT_NAME` store registers two events:

| Event | Synchronized boundary |
| --- | --- |
| `DS_EVENT_GAMEOBJECT_PRE_UPDATE` | After input dispatch, before `dmGameObject::Update`. |
| `DS_EVENT_GAMEOBJECT_POST_UPDATE` | After `dmGameObject::Update`, including late updates, before rendering. |

Their names are `"engine.gameobject.pre_update"` and
`"engine.gameobject.post_update"`. Depend on the corresponding event:

```cpp
DSSystemDesc desc = {
    .m_Name = dmHashString64("game.boids.steer"),
    .m_DependsOn = { DS_EVENT_GAMEOBJECT_PRE_UPDATE },
    .m_Query = boids_query,
    .m_Process = SteerBoids,
    .m_Context = boids_context,
};
DSResult result = DSRegisterFunction(store, &desc);
```

The engine begins one schedule per frame and calls `DSSchedulerSync` at each event
to finish ready callbacks and their dependents before continuing. The schedule
stays active across component updates and ends after the post-update stage.
Completed callbacks run once per frame; post-update callbacks may also depend on
pre-update callbacks. No query reservations remain during component updates, so
the main thread can add, remove and update rows there. Register/unregister
functions and destroy queries between frames, outside the active schedule.

The dedicated two-worker job context is separate from resource streaming.
The engine owns and destroys the datastore and its job context. Functions waiting
on custom unsignaled events are discarded when the frame's schedule ends.
Invalid dependency graphs are logged and skipped. Do not leave manual schedules
or query reservations active across these boundaries.

Callbacks run on workers when available; use datastore queries there. Apply
results to game objects on the main thread during component updates. These events
do not refresh world transforms: pre-update local transforms include earlier input
changes, while world transforms depend on normal hierarchy propagation. The
post-update event precedes `dmGameObject::PostUpdate` deletion processing.
Each event is once per frame, not once per collection or fixed physics step.

Standalone stores still use `DSRegisterEvent`, `DSBeginSchedule`,
`DSSchedulerEvent`, `DSSchedulerUpdate`, `DSSchedulerSync` and `DSEndSchedule` with a caller-owned job
context. Signaling only submits jobs; the caller must wait before accessing their
output. `DSEndSchedule` returns `BUSY` while jobs run or ready functions await
query access. Pump updates and retry it; waiting for `DSSchedulerUpdate` to return
`OK` would also wait for functions blocked on unsignaled events. End schedules
before destroying their job context or unregistering callbacks.

### Parallel access and structural changes

The library provides synchronization; the caller owns the job context. Registered
functions use scheduler-managed reservations; direct query users manage their own.
`DSQueryTryBegin` holds the store mutex while checking conflicts, refreshing row
ranges and recording an active reservation. It then releases the mutex, but the
reservation remains active throughout iteration. Structural operations acquire
the same mutex and return `DS_RESULT_LOCKED` while any reservation is active,
preventing row storage from moving or being freed. The caller finishes all jobs
before `DSQueryEnd` removes the reservation under the mutex. Structural changes
can succeed after the last reservation ends; rejected operations must be retried
by the caller.

There are no per-table mutexes. Reservations allow concurrent access to different
tables or non-overlapping fields in the same table.
Read/read overlaps are allowed; overlapping writes conflict. Reading a whole
inline struct conflicts with writing a member. Disjoint group filters can reserve
the same field concurrently; aliases of external views remain conservative.
Queries without requested fields or whole-row flags read only row identities,
so they do not conflict with value writes.
Explicit `DS_ROW_MUTABLE` reservations cover complete rows even when field
filters are present.

Explosion reads position and writes health. Regenerate also writes health,
increasing it up to 100. Whichever reserves access first runs; the other receives
BUSY and retries after release. Independent queries can proceed concurrently.
Each query handle has at most one active reservation, shared by its jobs.
Active reservations use `dmObjectPool` slots: admission scans a dense array and
release returns the query's slot directly. Query creation reserves pool capacity;
begin/end reuse it. Query pairs cache field-access compatibility until matching
tables change. Initial comparison intersects sorted table matches; row churn
refreshes ranges without discarding compatibility. This follows the instance-pool pattern in
[rig.cpp](../rig/src/rig.cpp). Query descriptors require valid caller storage; lookup does not repeat pointer validation.

![The caller submits jobs for admitted queries, waits for them, then releases
access. Conflicting updates retry; structural changes wait until queries end.](images/data-query-execution.svg)

Split the reserved row count into disjoint ranges, and give each job its own
`DSQueryIterRange` cursor. A range can split or cross table batches; it is not an
ID range or thread number. For 8,500 matches and 4,096 rows per job, submit
`(0, 4096)`, `(4096, 4096)` and `(8192, 308)`. The first begin after structural
changes rebuilds cached ranges and may allocate; later begins reuse them. Join all
jobs before `DSQueryEnd`, including when there are no matches.

Structural operations, query creation/destruction, serialization, ID-based writes
and resets reject changes while queries are active. String/container replacement
also stays outside reservations. Fixed-size fields need no per-row lock or atomic
operation. Unrelated ID-based access must not bypass query reservations.

The main thread may prepare resource blobs while jobs run, then stop admissions,
wait for queries to end and apply pending additions/removals. To remove rows found
during a scan, collect their `DSId`s and remove them afterward. Removal uses IDs
because dense row indices can change. Ending a query does not apply queued work.

Serial and parallel traversal use the same reservation. The caller or engine
scheduler begins access with `DSQueryTryBegin`, uses `DSQueryIter` for all matched
rows or `DSQueryIterRange` for job ranges, and calls `DSQueryEnd` after every
iterator and job finishes. Component callbacks receive admitted work; they do
not lock the store. Resets and string/container replacement happen afterward.

**Concurrency follow-up:** structural changes currently require every query in
the store to finish, even when they affect unrelated tables. Investigate allowing
additions/removals on unreserved tables while keeping active row storage, ID slots,
query bindings and shared resource pools stable. First-time compatibility checks
and range rebuilding after structural changes still run under the shared store
mutex. Keep field-level access reservations and caller-owned jobs; validate
any narrower synchronization with TSAN and concurrent spawn/despawn coverage.

Loaded blob handles use atomic reference counts, so separate stores can retain and
release the same resource concurrently. Keep a live caller reference during
registration and keep the immutable bytes alive through the last registration.

## Implementation

The public [datastore.h](../datastore/src/dmsdk/datastore/datastore.h) is the umbrella header. It includes the responsibility headers
below and [types.h](../datastore/src/dmsdk/datastore/types.h) for shared handles,
IDs, results and value types. Each public header can also be included directly.
Store, native table/row lifecycle, reset and opaque blob handles are public.
Serialization and generic construction values remain in the private [data.h](../datastore/src/data.h).

| Implementation | Public header | Responsibility |
| --- | --- | --- |
| [data.cpp](../datastore/src/data.cpp) | [datastore.h](../datastore/src/dmsdk/datastore/datastore.h) | Store creation/destruction. Native row lifecycle is declared in [table.h](../datastore/src/dmsdk/datastore/table.h); resource instances in [blob.h](../datastore/src/dmsdk/datastore/blob.h). |
| [data_query.cpp](../datastore/src/data_query.cpp) | [query.h](../datastore/src/dmsdk/datastore/query.h) | Query matching, field bindings, cached ranges and access reservations. |
| [scheduler.cpp](../datastore/src/scheduler.cpp) | [scheduler.h](../datastore/src/dmsdk/datastore/scheduler.h) | Named query callback registrations and copied dependency declarations. |
| [data_iter.cpp](../datastore/src/data_iter.cpp) | [iter.h](../datastore/src/dmsdk/datastore/iter.h) | Batch, row and field traversal, including iterator value access. |
| [data_field.cpp](../datastore/src/data_field.cpp) | [field.h](../datastore/src/dmsdk/datastore/field.h) | Value representation, construction validation, field get/set and reset. |
| [data_container.cpp](../datastore/src/data_container.cpp) | [container.h](../datastore/src/dmsdk/datastore/container.h) | Borrowed reads of struct members and mixed list elements. |
| [data_storage.cpp](../datastore/src/data_storage.cpp) | Internal | Value chunks, shared backing pages and empty-page reuse. |
| [data_payload.cpp](../datastore/src/data_payload.cpp) | Internal | Block allocation, payload recycling and reclamation. |
| [data_io.cpp](../datastore/src/data_io.cpp) | [blob.h](../datastore/src/dmsdk/datastore/blob.h) | Public blob loading and release; private serialization. |

The [library README](../datastore/README.md) describes building and running the
unit tests. Threading tests run under TSAN.
