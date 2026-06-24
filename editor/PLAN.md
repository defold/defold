# Plan: Graph-property GUI `node-msg`s

## Goal

Change GUI `node-msg` from a mostly protobuf-shaped map to a graph-property map.
Producers return graph property keys and raw graph values; save/build boundaries convert
that map to protobuf, clear defaults, and collapse custom properties.

Important terms:

- `selected-layout-values`: `GuiNode :prop->value`; current selected editor layout only.
- `layout-effective-values`: one `(layout->prop->value layout-name)` entry; valid for build.
- `layout-overrides`: one `(layout->prop->override layout-name)` entry; valid for save.
- `node-msg`: graph-property map where possible; structural protobuf/support keys may remain.

## Notes

`JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine'` is required for editor `lein` invocations.

We edit both Defold editor and extension-spine.

Review context:
- https://github.com/defold/extension-spine/pull/277
- https://github.com/defold/defold/pull/12457

## Core Design

- Keep `(static custom-property ...)` declarations as the generic source of GUI custom-property metadata.
- Do not add `custom-property-raw-values` or layout custom-property carrier outputs.
- `produce-*node-msg` functions stop calling `protobuf/make-map-without-defaults`.
- `produce-*node-msg` functions return graph property keys for GUI properties.
- `produce-gui-base-node-msg` emits graph property keys from explicit `^:raw`
  dependencies and does not read `GuiNode :prop->value`.
- Protobuf conversion happens in the save/build functions that know the context and sparsity rules.
- Custom properties are normal graph properties in `node-msg`; GUI collapses known custom-property keys into `:custom-properties` at conversion time.

`produce-spine-node-msg` should use `^:raw` Spine properties directly:

```clj
(g/defnk produce-spine-node-msg
  [visual-base-node-msg
   ^:raw spine-scene
   ^:raw spine-default-animation
   ^:raw spine-skin
   ^:raw spine-create-bones
   :as m]
  (coll/merge visual-base-node-msg (dissoc m :visual-base-node-msg)))
```

## Conversion Boundaries

Do not introduce a separate generic conversion helper that tries to handle every
context. Save, build, layout, and template code have different sparsity/default
rules, so each boundary should do the protobuf conversion it needs locally.
Small focused helpers are allowed for repeated mechanics such as custom-property
collapse, protobuf field conversion, and repeated-field assignment.

Common rules for these boundaries:

- Remove decorations such as `:layout->prop->override` and `:layout->prop->value` before serialization.
- Recognize known custom-property graph keys from `(static custom-property ...)`.
- Collapse those graph keys into a protobuf `:custom-properties` vector using the local context's sparse/complete rules.
- Convert remaining graph property keys through `property-conversions`.
- Treat remaining keys as existing protobuf/support keys.
- Apply default stripping only in the boundary that needs it.

## Save

- Normal scene node save converts each graph-property `node-msg` to protobuf and clears defaults there.
- Layout save converts `layout-overrides`, not `selected-layout-values`.
- Template override save converts the override graph-property map, then applies the existing template override filtering.
- Custom-property save output is sparse: include entries only when the graph property is non-default or explicitly overridden to default.
- Custom-property keys should not be added to regular `overridden-fields`; entry presence in `:custom-properties` is the custom-property override marker.

## Build

- Default runtime nodes convert the graph-property `node-msg` to runtime protobuf.
- Runtime layout nodes start from the graph-property `node-msg`, overlay the matching `layout-effective-values`, then convert to runtime protobuf.
- Detect layout custom-property changes from key presence in `layout-overrides`, not from value inequality.
- `make-rt-layout-desc` should:
  1. remove layout decorations;
  2. remove known custom-property graph keys and produce `:custom-properties` using statics;
  3. convert remaining graph keys using `property-conversions`;
  4. leave existing protobuf/support keys as-is.
- If a runtime layout changes any custom property, emit the complete effective custom-property vector for that node, because engine layout custom properties replace the default vector.
- Build/save must not use `selected-layout-values`.

## Load

- Keep `node-desc->node-properties` generic.
- Load expands protobuf `:custom-properties` into normal graph property values using the statics metadata.
- Generic custom-property sanitize stays: keep/sort known entries and drop entries whose ids or types do not match current statics.
- Missing `:custom-properties` entries are not synthesized on load; graph property defaults are the custom-property defaults.
- Layout/template override load preserves sparsity: present custom-property entries become graph property overrides.
- Legacy Spine fields still migrate into `custom_properties`.

## Extension-Spine

- Keep Spine properties as normal graph properties with `(static custom-property ...)`.
- `produce-spine-node-msg` emits raw graph property keys, not `:custom-properties`.
- Spine does not do `_this` / `_overridden-properties` branching for custom-property protobuf storage.
- Spine does not emit per-layout custom-property bookkeeping.

## Tests

- Top-level Spine save writes custom properties from raw graph values.
- Layout save writes sparse custom-property entries from `layout-overrides`.
- Template override save writes sparse custom-property entries.
- Runtime layout build writes a complete effective custom-property vector when a layout changes any custom property.
- Changing the selected editor layout does not affect save/build custom-property output.
- Custom-property defaults come from graph property defaults when entries are absent.
- Unknown or type-mismatched custom-property entries are dropped during sanitize.
- Resource rename updates Spine `:spine-scene` through the node-specific `update-gui-resource-reference`.
- Template runtime custom-property overrides preserve non-overridden base custom properties.
- Legacy Spine fields migrate to `custom_properties`.
- Run editor tests with:

```sh
JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine' lein test
```

## Acceptance Criteria

- `produce-gui-base-node-msg` has no dependency on `GuiNode :prop->value`.
- Producers do not call `protobuf/make-map-without-defaults`.
- `produce-spine-node-msg` emits graph property keys for Spine custom properties.
- Save/build conversion boundaries own protobuf/default/custom-property collapse.
- `git diff HEAD dev -- src/clj/editor/gui.clj` avoids selected-layout `prop->value` plumbing for custom-property save/build behavior.
