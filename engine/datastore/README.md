# Datastore

A C API for typed rows, field queries and caller-scheduled jobs. Include
`<dmsdk/datastore/datastore.h>` and link the `datastore` library. The library
uses dlib and has no dependency on game objects or third-party ECS libraries.

The engine provides a shared `HDataStore` through `DATASTORE_CONTEXT_NAME`.
Extensions borrow it and release their queries and tables before app finalization.
Standalone callers use `DataCreateStore()` and `DataDestroyStore()`.

- Register field layouts with `DataRegisterTable()`.
- Add native rows with `DataCreateRows()`, or field arrays with `DataCreateRowsSoA()`.
  Both copy input values into contiguous native rows.
- Create queries once, bind fields with `DataQueryFindField()`, and iterate using
  typed row getters.
- For jobs, reserve a query with `DataQueryTryBegin()`, split its rows with
  `DataQueryIterRange()`, join the jobs, then call `DataQueryEnd()`.
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
