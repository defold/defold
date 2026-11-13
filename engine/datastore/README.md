# Datastore

A C API for typed rows, field queries and job scheduling. Include
`<dmsdk/datastore/datastore.h>` and link the `datastore` library. The library
uses dlib and has no dependency on game objects or third-party ECS libraries.

Set `engine.datastore = 1` in `game.project` to enable the experimental engine
datastore; it is disabled by default. The engine provides its shared `HDataStoreContext`
through `DATASTORE_CONTEXT_NAME`.
Extensions borrow it and release their queries and tables before app finalization.
Standalone callers use `DataStoreCreate(0)` and `DataStoreDestroy()`. Pass `DataStoreCreateParams` to tune backing pages, row chunks and spare-page retention.

To remove the implementation from a custom engine, enable **Exclude Datastore**
in the editor's App Manifest properties. This replaces `datastore` with
`datastore_null`. The equivalent YAML is:

```yaml
platforms:
    common:
        context:
            excludeLibs: [datastore]
            libs: [datastore_null]
```

The null library contains only the engine lifecycle and scheduler entry points.
`DataStoreCreate()` and the engine context lookup return null; no datastore workers
are created. Extensions using row, field or query APIs require the full library.
With `engine.datastore = 0`, the implementation remains linked and standalone
stores are still available.

- Register field layouts with `DataStoreTableAdd()`.
- Pass a caller-owned group ID, such as the game-object handle, when adding rows.
  Group bookkeeping is created on first use; `0` means ungrouped rows.
- Add native rows with `DataStoreRowCreate()`, or field arrays with `DataStoreRowCreateSoA()`.
  Both copy input values into native rows stored in contiguous chunks.
- Remove one row with `DataStoreRowRemove()`, or all of a group's rows and blob
  registrations with `DataStoreGroupRemove()`, after query reservations finish.
- Combine `DataStoreTag("boid")` and `DataStoreField("position", DATASTORE_TYPE_VECTOR3, 0)` in
  `DataStoreQueryDesc.m_Items`; no item count is needed. Create queries once, bind fields
  with `DataStoreQueryFindField()`, and iterate using typed row getters.
- Register query callbacks with `DataStoreFunctionAdd()`. Begin a schedule with
  a caller-owned `HJobContext` and signal engine stages with `DataStoreSchedulerEvent()`.
  The scheduler submits ready jobs and reserves their query access until completion.
- For caller-dispatched jobs, reserve a query with `DataStoreQueryTryBegin()`, split its rows with
  `DataStoreQueryIterRange()`, join the jobs, then call `DataStoreQueryEnd()`.
  Retry rejected reservations and structural changes at the next safe point.

See [DATASTORE.md](../docs/DATASTORE.md) for the file format, layouts, ownership
and synchronization rules. Public headers document the API. Serialization and
generic construction values remain internal in `src/data.h`.

## Build and test

Follow the [engine build guide](../../README_BUILD.md) to configure the SDK.
From the repository root:

```sh
cmake -S . -B engine/build/arm64-macos \
  -DDEFOLD_SELECTED_ENGINE_LIBS=testmain,dlib,datastore
cmake --build engine/build/arm64-macos --target build_tests run_tests
```

Threading tests require a separate build configured with `-DWITH_TSAN=ON`,
including source-built dlib. See the [CMake guide](../../scripts/cmake/README.md)
for isolated build directories and platform selection.
