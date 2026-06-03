# GUI Custom Data Migration Plan

Context:
- Editor dir: `/Users/vlaaad/Projects/defold/editor`
- Defold repo root: `/Users/vlaaad/Projects/defold`
- Bob dir: `/Users/vlaaad/Projects/defold/com.dynamo.cr/com.dynamo.cr.bob`
- Spine extension repo: `/Users/vlaaad/Projects/extension-spine`
- Defold branch context: commits `25ac9ef127..e8527d4092`.
- Spine extension context: commits `94043b1..c5bcd21`.
- Goal: support custom GUI node data for extensions such as Onion without adding extension-specific fields or code to core editor GUI logic.

Refactoring direction:
- Move custom GUI property registration from the custom node type registration map into the `g/defnode` property definitions themselves.
- A custom GUI property should be a real layout graph property on the concrete node type, e.g. `:spine-scene`, with static metadata that declares its required protobuf custom property id, e.g. `"spine_scene"`.
- Editor scripts should use the normal graph property naming path, so `:spine-scene` is addressed as `spine_scene`; the protobuf/runtime id remains the explicit metadata value.
- The editor should inspect custom property metadata from the graph node type without creating a node instance, so this metadata must be static, not `dynamic`.
- This removes the hidden aggregate `:custom-properties` property and virtual `__...` properties from the editor-facing model.
- Save/load/build still need custom handling because all custom values serialize into the repeated `custom_properties` protobuf field and layout overrides still map to that single protobuf field number.
- Resource rename, template overrides, and layout override serialization should operate on the real graph property keys, using the static custom property metadata only when converting to or from protobuf custom property entries.

# Using lein

Note: currently, ALL lein invocation have to be prefixed with `JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine'`, e.g.:

```sh
# running tests
JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine' lein test
# repl
JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine' lein run -m clojure.main
```
# Refactoring plan

## Summary

Refactor custom GUI properties from virtual `__...` properties backed by hidden `:custom-properties` storage into real layout graph properties declared on the custom node type. The protobuf/runtime custom property id remains explicit static metadata on each graph property. Since this branch is unreleased, remove branch-only compatibility paths such as `__spine_scene` editor-script aliases and `custom-property-dynamics`.

## Ordered implementation

1. Add a `static` directive to `g/defnode` properties and document it:
   ```clojure
   (static custom-property {:id "spine_scene"
                            :protobuf-type :type-hash
                            :resource-kind :spine-scene})
   ```
   Store it on the static property definition, readable from node type metadata without constructing a node.
2. Add GUI helpers that collect custom property metadata from node type properties into `prop-kw -> info` and `id -> info`.
3. Convert fake test custom GUI nodes and Spine GUI node declarations to real graph properties with `(static custom-property ...)`.
4. Change `register-custom-node-type-info` to stop accepting `:custom-properties`; inspect `:node-cls` static metadata instead and validate duplicate protobuf ids.
5. Replace save/load/build conversion to use real graph property keys plus static metadata.
6. Remove hidden/virtual editor code: `:custom-properties` graph storage, `__...` exposure, `custom-property-dynamics`, special editor-script parsing, and attachment splitting.
7. Add a `custom-property-values` output or equivalent dependency hook so `node-msg` invalidates when any real custom property changes.
8. Do a diff pass against `dev` and remove leftovers whose only purpose was the virtual/aggregate model.

## Behavior

- Require `:id` and `:protobuf-type` in custom property metadata; do not derive or fall back for the protobuf id.
- Use the graph property declaration for property type, default, label, edit type, error, and layout behavior.
- Load `custom_properties` entries into real graph properties by metadata id.
- Save/build real graph properties back into protobuf `custom_properties`.
- Keep runtime/build conversion from string `id` to `id_hash`, including hash-valued property conversion.
- Keep layout override serialization mapped to protobuf field `custom_properties` / field number 50.
- Editor scripts use normal names like `spine_scene`; old `__spine_scene` names are intentionally unsupported.
- Resource rename and template override transfer operate on real graph property keys, using metadata only for protobuf conversion.

## Acceptance criteria

- `gui.clj` diff against `dev` is smaller than the baseline at `8dc08cee83` (`bring back plan`), where `dev...HEAD` showed 1736 changed lines in `gui.clj`.
- `gui.clj` no longer contains `custom-property-id-prefix`, `custom-property-dynamics`, virtual `__...` handling, or a `GuiNode` property named `custom-properties`.
- Custom GUI project files still load/save with protobuf `custom_properties` string ids.
- Editor build output still emits runtime custom properties with `id_hash`, not string `id`.
- Default-valued custom property layout overrides round-trip and remain marked overridden.
- Resource renames update custom resource-name properties in default layout and named layout overrides.
- Editor scripts can list/get/set/reset custom GUI properties using normal names like `spine_scene`.
- Template override transfer works for individual real custom graph properties.

## Tests

- Graph: add a `defnode` test for `(static custom-property ...)` metadata on `g/declared-properties`.
- GUI editor: update custom GUI tests for load/save, sparse saves, layout overrides, template override transfer, resource rename, and editor-script access without `__`.
- Spine: update integration expectations from `:__spine_scene` / `"__spine_scene"` to `:spine-scene` / `"spine_scene"`.
- Save/build: keep coverage for legacy Spine field migration, readable project custom property ids, runtime `id_hash`, and hash-valued custom properties.

## Assumptions

- No compatibility is needed for APIs introduced only on this branch.
- Existing released legacy Spine GUI fields still need migration support.
- Public project/runtime wire format remains `custom_properties`; only editor graph representation changes.
