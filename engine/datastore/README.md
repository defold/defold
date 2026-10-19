# Datastore

A C API for typed rows, field queries and caller-scheduled jobs. Include
`<dmsdk/datastore/datastore.h>` and link the `datastore` library. The library
uses dlib and has no dependency on game objects or third-party ECS libraries.

The engine provides a shared `HDSContext` through `DATASTORE_CONTEXT_NAME`.
Extensions borrow it and release their queries and tables before app finalization.
Standalone callers use `DSCreateStore(0)` and `DSDestroyStore()`. Pass `DSCreateParams` to tune backing pages, row chunks and spare-page retention.

- Register field layouts with `DSRegisterTable()`.
- Add native rows with `DSCreateRows()`, or field arrays with `DSCreateRowsSoA()`.
  Both copy input values into native rows stored in contiguous chunks.
- Combine `DSTag("boid")` and `DSField("position", DS_TYPE_VECTOR3, 0)` in
  `DSQueryDesc.m_Items`; no item count is needed. Create queries once, bind fields
  with `DSQueryFindField()`, and iterate using typed row getters.
- For jobs, reserve a query with `DSQueryTryBegin()`, split its rows with
  `DSQueryIterRange()`, join the jobs, then call `DSQueryEnd()`.
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
