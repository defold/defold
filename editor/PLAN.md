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

# Bob

To compile and test bob, start a shell in defold (parent) folder:
```sh
./scripts/build.py shell
```

Then, in the shell, build:
```sh
DM_BOB_BUNDLERTEST_ONLY_HOST=1 ./scripts/build.py build_bob
```


Then, in the shell, in editor folder, do `lein init` so editor can use the new bob code. Then, `lein test` will use the new bob code (see note on required `JVM_OPTS` in PLAN.md)

# Refactoring plan

1. Add explicit regression tests for custom property override merging:
   - Editor integration test: a templated custom GUI node has two non-default
     custom properties in the source; the referencing scene overrides only one
     property; saved/build output preserves the other property.
   - Bob test: the same sparse template override case builds correctly without
     replacing the whole `custom_properties` list.

2. Fix Bob override application if needed:
   - Keep normal protobuf field override behavior for regular fields.
   - Special-case `NodeDesc.custom_properties` field 50 to merge by custom
     property id/hash, so one overridden custom property does not clear others.

3. Re-run focused editor and Bob tests:
   - Editor GUI/custom property tests with the Spine extension path in
     `JVM_OPTS`.
   - Bob GUI builder tests from the `./scripts/build.py shell` environment.

4. Do final cleanup pass:
   - Remove planning-only or temporary branch artifacts if they should not ship.
   - Re-check `git diff dev --stat` and ensure remaining changes map to the
     refactor or its tests.
