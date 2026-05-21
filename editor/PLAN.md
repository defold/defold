# GUI Custom Data Migration Plan

Context:
- Editor dir: `/Users/vlaaad/Projects/defold/editor`
- Defold repo root: `/Users/vlaaad/Projects/defold`
- Bob dir: `/Users/vlaaad/Projects/defold/com.dynamo.cr/com.dynamo.cr.bob`
- Spine extension repo: `/Users/vlaaad/Projects/extension-spine`
- Defold branch context: commits `25ac9ef127..e8527d4092`.
- Spine extension context: commits `94043b1..c5bcd21`.
- Goal: support custom GUI node data for extensions such as Onion without adding extension-specific fields or code to core editor GUI logic.

Current state:
- `Gui$NodeDesc` still has deprecated Spine fields: `spine_scene`, `spine_default_animation`, `spine_skin`, `spine_create_bones`, `spine_node_child`.
- Bob already has generic GUI custom property support via `GuiCustomNode`, `IGuiCustomNode`, and `IGuiCustomType`.
- `extension-spine` declares Spine GUI fields in `defold-spine/pluginsrc/com/defold/extension/pipeline/SpineGuiNode.java`.
- Editor Spine GUI code is in `defold-spine/editor/src/spineguiext.clj`.

Direction:
- Use generic `custom_properties` storage on `Gui$NodeDesc`.
- Project files use string `id`; runtime/build output uses `id_hash`.
- Use `custom_type_name` as editor/project-file metadata for custom node type names.
- Values should be typed with a protobuf `oneof`.
- Do not add Onion-specific or future extension-specific fields to `Gui$NodeDesc`.

Editor approach:
- Keep core `gui.clj` generic; it must not know keys like `spine_scene`.
- Let custom GUI node types declare fields through metadata.
- Reuse the Java `IGuiCustomType` contract in the editor by constructing a small collector and invoking static `registerProperties(IGuiCustomType)`.
- Use collected metadata in `node-desc->node-properties` to extract custom values from `:custom-properties`.
- Use the same metadata when producing node messages to store declared custom properties into `:custom-properties`.
- Sort `custom_properties` deterministically by `id` in the generic helper.

Spine migration:
- Leave only compatibility code in `spineguiext`.
- On load/sanitize, migrate old Spine fields into `:custom-properties`: `spine_scene`, `spine_default_animation`, `spine_skin`, `spine_create_bones`.
- Drop `spine_node_child`; it is legacy bone-node data.
- Update `produce-spine-node-msg` to emit `custom_properties`, not old Spine fields.
- Keep existing editor-facing Spine graph properties for now so validation, layouts, and UI behavior continue to work.

Bob/runtime:
- Bob should continue accepting legacy Spine fields and build them as custom fields for compatibility.
- Built data should replace string ids with id hashes.
- Runtime custom GUI node code should read custom fields only.

# Using lein

Note: currently, ALL lein invocation have to be prefixed with `JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine'`, e.g.:

```sh
# running tests
JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine' lein test
# repl
JVM_OPTS='-Ddefold.extension.spine.path=/Users/vlaaad/Projects/extension-spine' lein run -m clojure.main
```

# Implementation

## 1. Add GUI resource-type-backed extension registration.

- Replace global `node-type-info-registry` with registry metadata stored on the `.gui` resource type map.
     ```clojure
     {:custom-type-name->type-info {"Spine" spine-type-info}
      :node-cls->custom-type-name {SpineNode "Spine"}}
     ```
- Remove `custom-gui-scene-loaders`; register GUI resource kinds instead.
- Add transaction-returning APIs:
     ```clojure
     (gui/register-node-type-info workspace type-info)
     (gui/register-gui-resource-kind workspace resource-kind info)
     ```
- Both APIs update the `.gui` resource type in `:resource-types` and `:resource-types-non-editable`.
- Updating registered GUI node type info regenerates the `.gui` resource type `:read-fn`, so the sanitize function stays pure and captures a registry snapshot instead of receiving a workspace or querying graph node values.
- Register custom node type info with `custom-type-name`; derive `custom-type` with `murmur/hash32` when needed.
     ```clojure
     {:custom-type-name "Spine" ;; Required.
      :node-cls SpineNode ;; Required.
      :display-name "Spine" ;; Required. Can be localization message.
      :icon spineext/spine-scene-icon ;; Required.
      :defaults gui/visual-base-node-defaults ;; Required.
      :convert-fn fixup-spine-node ;; Optional. Legacy migration hook.
      :custom-properties
      [{:id :spine_scene ;; Required.
        :type g/Str ;; Required. Editor value type.
        :protobuf-type :hash ;; Optional. Derived from :type if omitted.
        :default "" ;; Optional. Derived from :type if omitted.
        :label "Spine Scene" ;; Optional. Can be localization message.
        :resource-kind :spine_scene ;; Optional. Scene-local GUI resource name string.
        :edit-type-fnk ... ;; Optional. Derived from :type and :resource-kind if omitted.
        :error-fnk ...} ;; Optional.
       {:id :spine_default_animation ;; Required.
        :type g/Str ;; Required.
        :protobuf-type :hash ;; Optional. Stored as string in project files, hashed during build.
        :default "" ;; Optional. Derived from :type if omitted.
        :label "Default Animation" ;; Optional. Can be localization message.
        :edit-type-fnk ... ;; Optional. Derived from :type and :resource-kind if omitted.
        :error-fnk ...} ;; Optional.
       {:id :spine_skin ;; Required.
        :type g/Str ;; Required.
        :protobuf-type :hash ;; Optional. Stored as string in project files, hashed during build.
        :default "" ;; Optional. Derived from :type if omitted.
        :label "Skin" ;; Optional. Can be localization message.
        :edit-type-fnk ... ;; Optional. Derived from :type and :resource-kind if omitted.
        :error-fnk ...} ;; Optional.
       {:id :spine_create_bones ;; Required.
        :type g/Bool ;; Required. Editor value type.
        :protobuf-type :boolean ;; Optional. Derived from :type if omitted.
        :default false ;; Optional. Derived from :type if omitted.
        :label "Create Bones" ;; Optional. Can be localization message.
        }]}
     ```
     If omitted, derive `:protobuf-type` from `:type`: `g/Str -> :string`, `g/Bool -> :boolean`, `g/Num -> :number`. For `:protobuf-type :hash`, project files still store readable strings; build output hashes the value.
     If omitted, derive `:edit-type-fnk` from `:type`; when `:resource-kind` is present, use a resource-name choice edit type backed by the matching GUI resource folder.
- Store GUI resource kinds in the `.gui` resource type map next to the GUI node type registry.
     ```clojure
     {:spine_scene
      {:label "Spine Scenes" ;; Required. Folder label.
       :icon spineext/spine-scene-icon ;; Required.
       :exts ["spinescene"] ;; Required. Selects matching Gui$ResourceDesc paths.
       :node-type SpineSceneNode ;; Required. Scene-local GUI resource entry node type.
       :resource-property :spine-scene ;; Required. Property receiving the resolved resource.
       :attachment-property :spine-scenes ;; Required. Editor script property; Lua uses "spine_scenes".
       :attach-fn attach-spine-scene}} ;; Required. Existing fn: gui-scene, folder-node, entry-node -> txs.
     ```
- Validate and normalize `register-gui-resource-kind` info immediately, including expanding `:exts` to a vector and rejecting missing or invalid required fields.
- Add generic `GuiResourceKindNode` for resource folders. During GUI load, create one folder per registered resource kind, create entry nodes from matching `SceneDesc.resources` by `:exts`, set `:name` and `:resource-property`, and attach entries with `:attach-fn`.
- Keep all `GuiResourceKindNode` graph properties hidden; it is an internal folder node, not user-editable resource data.
- Replace `SpineScenesNode` with `GuiResourceKindNode`; keep `SpineSceneNode` and `attach-spine-scene`.
- Initialize built-in GUI node type infos in the `.gui` resource type registration metadata.
- Add generic editor tests with a fake custom GUI node type and resource kind to cover registration metadata, `.gui` editable/non-editable resource type updates, load/save using numeric `custom_type`, generic resource kind load/save, and deterministic property/resource order.
- In the Part 1 tests, assert `custom_type_name` registration derives the expected numeric `custom_type` and stores the name mappings, but do not require project files to load from `custom_type_name` yet.

## 2. Move custom GUI field storage to layout-aware `:custom-properties`.

- Add a generic `:custom-properties` graph property to custom GUI nodes.
- Store values as an `id -> value` map; use registered metadata for `id -> type`.
- Do not add extension-specific protobuf-backed storage fields for custom data. Extension nodes may still expose editor-facing computed properties and outputs derived from `:custom-properties`.
- Store layout overrides sparsely under `:layout->prop->override layout :custom-properties`, with only overridden custom property ids present.
- Merge effective custom property values by id from defaults, loaded project values, original-node overrides, current-node overrides, and current-layout overrides.
- Load default and layout `custom_properties` entries into the value map.
- Save node and layout custom properties sparsely: omit default-valued custom properties from project files so load/sanitize migrations do not dirty resources.
- Save layout nodes with only overridden custom property entries. `:custom-properties` should contribute one symbolic overridden field entry even when several custom ids are overridden.
- Deduplicate `:overridden-fields` after custom property expansion.
- Update GUI resource rename handling to rewrite matching values inside the `:custom-properties` map, including layout overrides.
- Drop unknown custom property ids during load sanitization and save. For known ids, fail fast with a useful load error if the protobuf value type disagrees with registered metadata.
- Migrate legacy Spine fields into `:custom-properties`: `spine_scene`, `spine_default_animation`, `spine_skin`, `spine_create_bones`; drop `spine_node_child`.
- Add editor tests for load/save, sparse layout overrides, override field deduplication, resource renames, unknown-id round-tripping, and Spine legacy field migration.

Part 2 implementation notes:
- Implemented using protobuf enum keywords for `:protobuf-type`, e.g. `:type-string`, `:type-boolean`, `:type-number`, `:type-hash`, `:type-vector3`, `:type-vector4`, and `:type-quat`.
- Unknown-id round-tripping is no longer a goal; unknown entries are dropped instead.
- Spine legacy migration is hardcoded in `extension-spine` and remains non-extensible compatibility code.
- Current editor tests cover sparse node saves, sparse layout overrides, default omission, resource renames, dropped unknown ids, dirty-on-load behavior for legacy Spine data, and legacy Spine field migration.

## 3. Expose custom properties in editor tools.

- Add virtual custom property entries to the GUI node `:_properties` output so they appear in the Properties panel as flat layout properties keyed by custom property id, not legacy extension graph properties.
- Keep the graph storage property nested as `:custom-properties {id value}`, but expand it early into flat virtual property keys for layout-facing maps such as `prop->value` and `layout->prop->value`.
- Custom property edits and clears must route by custom property id: default-layout edits update the nested `:custom-properties` graph property, while named-layout edits write flat custom property id keys directly under `:layout->prop->override layout`.
- Save/build must collapse flat custom property id keys back into protobuf `custom_properties`; layout saves must include every overridden custom property id entry, even when its value equals the custom property default.
- Add a regression test for default-valued custom property layout overrides. The Part 2 nested representation can lose `{"Landscape" {:custom-properties {:some_id default-value}}}` because `custom_properties` is only one protobuf overridden field and sparse serialization can omit the per-id entry; Part 3's flat per-id override keys must round-trip this case and save the overridden id explicitly.
- Reject custom property ids that collide with real layout property keys during custom GUI node type registration.
- After virtual custom property editing works by id, remove the generic `:prop-key` custom-property metadata and save-time prop-key migration helpers such as `move-custom-property-prop-keys-to-custom-properties`; layout serialization should no longer translate editor graph property keys like `:spine-scene` into custom property ids like `:spine_scene`.
- Revisit the custom-property-aware layout serialization helpers such as `prop-entry->pb-field-entry-for-type` and `prop->pb-field-entries-for-type`; after save/build collapse flat custom id keys explicitly, either rename them to describe layout property serialization clearly or inline/simplify them if they no longer carry meaningful logic.
- Revisit the hidden `:custom-properties` edit type. It currently has a dummy `g/Any` edit type because existing layout/editor-script helpers assume every layout property has one; after virtual custom property editing is implemented, remove or replace this dummy storage-property edit type as part of the new custom-property edit path.
- Extend editor script property lister/getter/setter/resetter for GUI nodes to include these virtual custom properties.
- Include virtual custom properties in template override transfer so GUI template workflows can copy and clear individual custom property overrides.
- Add editor tests for Properties panel metadata, editor script lister/getter/setter/resetter access, and template override transfer for individual custom property ids.

## 4. Save readable custom GUI data from the editor.

- Save project files with readable `custom_type_name`.
- Save custom properties with string `id` and readable project values. (The string `id` storage part is already covered by Part 2.)
- Accept old files with numeric `custom_type`; migrate known Spine hash to `custom_type_name` `"Spine"` on save.
- Fail fast if both `custom_type_name` and `custom_type` are present but disagree.
- Update `save_data_test` classification for `custom_type_name` and `custom_properties`. (`custom_properties` coverage is already partly covered by Part 2.)
- Update `save_data_test` Spine GUI fixtures to the new project-file format. (The `custom_properties` fixture migration is already partly covered by Part 2; `custom_type_name` remains pending.)
- Add editor save tests for loading/saving readable `custom_type_name` without numeric `custom_type`, old numeric Spine custom type migration, mismatched `custom_type_name`/`custom_type` rejection, and readable source output.
- Update the fake custom GUI node/resource kind test, or add a sibling test, to use `custom_type_name` once project-file loading from `custom_type_name` is implemented.

## 5. Build runtime custom GUI data from the editor.

- In `node-desc->rt-node-desc`, remove `custom_type_name`.
- Resolve `custom_type_name` to `custom_type` before node type lookup and runtime output.
- Convert each custom property string `id` to `id_hash` using `murmur/hash64`.
- Clear string `id` from runtime/build output.
- Convert custom properties with `:protobuf-type :hash` from readable project strings to hashed runtime values.
- Sort `custom_properties` deterministically in editor build output. (Editor save/build paths already sort by string `id`; id/hash conversion remains pending.)
- Add editor build tests for `custom_type_name` removal, `id -> id_hash` conversion, hash-valued custom properties, and deterministic property order.

## 6. Match the same build behavior in Bob and extension-spine.

- Bob resolves `custom_type_name` to `custom_type` before custom type lookup, and rejects mismatches when both are present.
- Bob accepts project files with string `id` custom properties, emits `id_hash` for runtime, clears editor-only ids, and sorts properties deterministically.
- Bob continues accepting old Spine fields and converts them to custom properties.
- `extension-spine` runtime reads custom properties only.
- Add Bob tests for `custom_type_name` resolution, string `id` to `id_hash` conversion, deterministic order, and legacy Spine input.
