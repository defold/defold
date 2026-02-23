# Plan: `editor.properties(node)` for Editor Scripts

## Goal

Implement `editor.properties(node)` so editor scripts can discover readable properties at runtime, addressing [defold/defold#11902](https://github.com/defold/defold/issues/11902).

## Scope (MVP)

- Add `editor.properties(node)` returning `string[]` (property names).
- Return properties that are currently readable in context (i.e. `editor.get(node, prop)` would succeed).
- Keep existing property naming conventions (`snake_case`, `__internal`, layout-prefixed keys such as `Landscape:position`).
- Return deterministic output (sorted, deduplicated).

## Implementation Steps

1. API wiring in `editor_extensions.clj`
- Add `make-ext-properties-fn` next to `make-ext-get-fn` / `make-ext-can-get-fn`.
- Coerce input with existing `graph/node-id-or-path-coercer`.
- Resolve node/path via `graph/resolve-node-id-or-path`.
- Return Lua array of strings from a new graph helper.
- Register in runtime env:
  - `src/clj/editor/editor_extensions.clj` under `"editor"` map as `"properties"`.

2. Property enumeration in `graph.clj`
- Add `ext-readable-properties` function in `src/clj/editor/editor_extensions/graph.clj`.
- For resources:
  - Folder resource: `["path" "children"]`
  - File resource: `["path"]`
- For graph nodes: union of
  - Built-in readable properties (e.g. `"type"`).
  - Registered non-outline getter-backed properties (e.g. `"text"`, `"tiles"`, `"tile_collision_groups"`, etc.).
  - Visible outline properties with supported conversion.
  - Attachment list properties available on this node (including alternative-chain resolution).
  - Dynamic `game.project` keys from form/meta settings as `section.key`.
- Deduplicate and sort before returning.

3. Make getter registration introspectable
- Extend registration structure so each registered getter can also provide a property list (static vector or function).
- Update existing `register-property-getter!` usages to specify discoverable property names once.
- Use that metadata both for lookup (`editor.get`) and introspection (`editor.properties`), avoiding duplicated property-name logic.

4. Documentation updates
- Update `src/clj/editor/editor_extensions/docs.clj`:
  - Add `editor.properties` function docs near `editor.get` / `editor.can_get`.
  - Mention context sensitivity (list may vary by node/resource and editor state).

5. Integration tests
- Add tests under `test/integration/editor_extensions_test.clj` plus a dedicated test resource project/script.
- Verify behavior for:
  - Folder resource (`/assets`)
  - File resource (`/game.project` or text resource)
  - Typical outline node (e.g. sprite/go child)
  - Node exposing attachment list properties
  - `game.project` node dynamic keys
- Assertions:
  - Returned list is sorted and unique.
  - Expected key subsets are present.
  - Every returned key satisfies `editor.can_get(node, key) == true`.

## Follow-Up (Non-MVP)

- Add `editor.describe(node)` returning metadata table per property:
  - `name`, `readable`, `writable`, possibly `resettable`, `list`, and coarse `type`.
- Reuse the same enumeration backbone introduced for `editor.properties(node)`.
