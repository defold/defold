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

# Implementation

## 1. Add Workspace-backed GUI extension registration.

- Replace global `node-type-info-registry` with Workspace property `:gui-node-type-registry`.
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
- Store GUI resource kinds in Workspace property `:gui-resource-kind-registry`.
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
- Add generic `GuiResourceKindNode` for resource folders. During GUI load, create one folder per registered resource kind, create entry nodes from matching `SceneDesc.resources` by `:exts`, set `:name` and `:resource-property`, and attach entries with `:attach-fn`.
- Replace `SpineScenesNode` with `GuiResourceKindNode`; keep `SpineSceneNode` and `attach-spine-scene`.
- Initialize built-in GUI node type infos in `:gui-node-type-registry`.
- Add generic editor tests with a fake custom GUI node type and resource kind to cover registration, load, save, and deterministic property/resource order.

## 2. Move custom GUI field storage to layout-aware `:custom-properties`.

- Add a generic `:custom-properties` graph property to custom GUI nodes.
- Store values as an `id -> value` map; use registered metadata for `id -> type`.
- Do not add extension-specific protobuf-backed storage fields for custom data. Extension nodes may still expose editor-facing computed properties and outputs derived from `:custom-properties`.
- Store layout overrides sparsely under `:layout->prop->override layout :custom-properties`, with only overridden custom property ids present.
- Merge effective custom property values by id from defaults, loaded project values, original-node overrides, current-node overrides, and current-layout overrides.
- Load default and layout `custom_properties` entries into the value map.
- Save default nodes with all custom property entries.
- Save layout nodes with only overridden custom property entries. `:custom-properties` should contribute one symbolic overridden field entry even when several custom ids are overridden.
- Deduplicate `:overridden-fields` after custom property expansion.
- Update GUI resource rename handling to rewrite matching values inside the `:custom-properties` map, including layout overrides.
- Preserve unknown custom property ids when round-tripping. For known ids, fail fast with a useful load error if the protobuf value type disagrees with registered metadata.
- Migrate legacy Spine fields into `:custom-properties`: `spine_scene`, `spine_default_animation`, `spine_skin`, `spine_create_bones`; drop `spine_node_child`.
- Add editor tests for load/save, sparse layout overrides, override field deduplication, resource renames, unknown-id round-tripping, and Spine legacy field migration.

## 3. Expose custom properties in editor tools.

- Add virtual custom property entries to the GUI node `:_properties` output so they appear in the Properties panel and edit `:custom-properties`.
- Extend editor script property lister/getter/setter/resetter for GUI nodes to include these virtual custom properties.
- Include virtual custom properties in template override transfer so GUI template workflows can copy and clear individual custom property overrides.
- Add editor tests for Properties panel metadata, editor script lister/getter/setter/resetter access, and template override transfer for individual custom property ids.

## 4. Save readable custom GUI data from the editor.

- Save project files with readable `custom_type_name`.
- Save custom properties with string `id` and readable project values.
- Accept old files with numeric `custom_type`; migrate known Spine hash to `custom_type_name` `"Spine"` on save.
- Fail fast if both `custom_type_name` and `custom_type` are present but disagree.
- Update `save_data_test` classification for `custom_type_name` and `custom_properties`.
- Update `save_data_test` Spine GUI fixtures to the new project-file format.
- Add editor save tests for old numeric Spine custom type migration, mismatched `custom_type_name`/`custom_type` rejection, and readable source output.

## 5. Build runtime custom GUI data from the editor.

- In `node-desc->rt-node-desc`, remove `custom_type_name`.
- Resolve `custom_type_name` to `custom_type` before node type lookup and runtime output.
- Convert each custom property string `id` to `id_hash` using `murmur/hash64`.
- Clear string `id` from runtime/build output.
- Convert custom properties with `:protobuf-type :hash` from readable project strings to hashed runtime values.
- Sort `custom_properties` deterministically in editor build output.
- Add editor build tests for `custom_type_name` removal, `id -> id_hash` conversion, hash-valued custom properties, and deterministic property order.

## 6. Match the same build behavior in Bob and extension-spine.

- Bob resolves `custom_type_name` to `custom_type` before custom type lookup, and rejects mismatches when both are present.
- Bob accepts project files with string `id` custom properties, emits `id_hash` for runtime, clears editor-only ids, and sorts properties deterministically.
- Bob continues accepting old Spine fields and converts them to custom properties.
- `extension-spine` runtime reads custom properties only.
- Add Bob tests for `custom_type_name` resolution, string `id` to `id_hash` conversion, deterministic order, and legacy Spine input.
