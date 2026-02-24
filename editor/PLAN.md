# Plan: `editor.properties(node)` for Editor Scripts

## Maintenance Note

- Keep this file up to date whenever code or documentation changes affect plan status.

## Goal

Implement `editor.properties(node)` so editor scripts can discover readable properties at runtime, addressing [defold/defold#11902](https://github.com/defold/defold/issues/11902).

## Target

- Add `editor.properties(node)` returning `string[]` (property names).
- Return properties that are currently readable in context (i.e. `editor.get(node, prop)` would succeed).
- Keep existing property naming conventions (`snake_case`, `__internal`, layout-prefixed keys such as `Landscape:position`).
- Return deterministic output (sorted, deduplicated).

## Current State (Branch: `issue-11902-properties-list`)

### Done

1. API wiring in `editor_extensions.clj`
- Added `make-ext-properties-fn`.
- Coerces input with `graph/node-id-or-path-coercer`.
- Resolves node/path via `graph/resolve-node-id-or-path`.
- Registered in runtime env under `"editor"` as `"properties"`.

2. Property enumeration backbone in `graph.clj`
- Added `ext-readable-properties`.
- Added introspectable getter/lister registration:
  - `ext-property-lister`
  - `register-property-getter!` arity with optional lister fn.
- Added listers for current getter-backed property groups, including dynamic `game.project` keys and visible+convertible outline properties.
- Includes attachment list property names and traverses alternatives.
- Returns sorted, deduplicated output.

3. Attachment traversal utilities
- Replaced one-off alternative lookup utility with reducible alternative-chain traversal (`alternatives`).
- Added `attachment/list-kws` for list property discovery across alternatives.

4. Related test maintenance
- Updated `make-inheritance-chain` call sites in `editor_extensions_test.clj` to new kv-arg API.

5. Documentation
- Updated `src/clj/editor/editor_extensions/docs.clj`:
  - Added `editor.properties` docs near `editor.get` / `editor.can_get`.
  - Documented context sensitivity (list varies by node/resource/editor state).
  - Clarified that listed properties are editor properties and may support transaction operations depending on `editor.can_*`.

### Clarification

- Resource behavior follows existing `editor.get` semantics:
  - Resource path supports `"path"`.
  - Folder resources additionally support `"children"`.

## Remaining Work

1. Integration tests for `editor.properties`
- Add/extend tests under `test/integration/editor_extensions_test.clj` (and test resources if needed).
- Verify at least:
  - Folder resource (`/assets`): includes `path`, `children`.
  - File resource (`/game.project` or text file): includes `path`.
  - Typical outline node (e.g. sprite/go child): expected readable keys.
  - Node with attachment list properties.
  - `game.project` node dynamic `section.key` entries.
- Assertions:
  - Returned list is sorted and unique.
  - Expected key subsets are present.
  - Every returned key satisfies `editor.can_get(node, key) == true`.
