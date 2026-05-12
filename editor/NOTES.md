SPINE

Ok, here is the current progress of the Gui/Spine task:
- The Bob+Engine branch spine-gui-removal: https://github.com/defold/defold/pull/12375 (latest is e8527d409233fb9e640601eb77ea60f59692cef0)
- The Spine branch gui-properties-update: editor plugins build with e8527d409233fb9e640601eb77ea60f59692cef0
These changes follow the design document we created.
The spine plugin pluginSpineExt.jar registers its custom Gui node, and you’ll have to modify the Bob api to retrieve the information in the way you need.

Note that if you need to rebuild pluginSpineExt.jar, you need to use the “DEFOLDSDK=<the engine sha1> ./utils/build_plugins.sh” in the spine extension. But, unless you change the custom gui node type api, you shouldn’t have to.




### [7] Editor-prep - Have to do:
* [3] Prepare `gui.clj` for custom gui resources. Currently, a lot of Spine-specific connections remain that channel data from the `SpineModelNode` (resource) to the `SpineNode` (scene object).
    * `gui-resources` should be provided by the `GuiSceneNode` to all `GuiNodes` below it. Consolidate all resource info into a shared map instead of separate `spine-scene-infos`.
    * `update-gui-resource-references` categorizes resource types by `TGuiResourceType`. Instead of a hard-coded set of resource types, this should be dynamic.
* [4] Store Onion-specific properties in the `Gui$NodeDesc`. Currently, this contains a bunch of Spine-related properties, and we don't want to add Onion-related properties to it.
    * [3] Editor changes
        * Use the `custom_properties` `repeated` field on `Gui$NodeDesc`, where each entry has an `optional string id` for the project files, an `optional int id_hash` for the compiled runtime binaries, and a `oneof` `value` field that can be a `string`, `float` `hash`, or whatever we need (design doc pending).
        * In `node-desc->node-properties`, introduce an extension point so that the `OnionNode` (scene object) can extract additional node-property kv-pairs from the `custom_properties`.
        * When producing the `node-msg` add entries to `custom_properties` in the `Gui$NodeDesc`.
            * It would be good to keep this list sorted. Should the plugin be responsible for this?
        * In `node-desc->rt-node-desc`, replace `id` with `id_hash` for all `custom_properties` entries.
        * Add tests for `custom_properties` in the editor.
    * [1] Bob changes
        * Update the corresponding `Gui$NodeDesc` building code in Bob to build the `custom_properties`, convert `string id` into `int id_hash`, and so on.
        * Add tests for `custom_properties` in Bob.

### [8] Editor-prep - Really want to do:
* [3] Migrate Spine-related fields in existing `Gui$NodeDescs` to `custom_properties`?
    * [2] Editor changes
        * Update `spineguiext/fixup-spine-node` to move values from `:spine-scene`, `:spine-default-animation`, `:spine-skin`, and `:spine-create-bones` into `:custom-properties` entries with string `id`s.
        * Update `spineguiext/produce-spine-node-msg` to store these values as `:custom-properties` with string `id`s.
        * Add migration tests to the editor.
        * Update spine-referencing gui scenes in the `save_data_test` project to the new format.
    * [1] Bob changes
        * Update Bob to be able to build spine-related fields in existing `Gui$NodeDesc` project content as `custom_properties`.
* [2] Use `custom_type_name` alongside `custom_type` so project files are nicer for humans to read. Can still hash to an `int` for the runtime.
    * `custom_type` remains the numeric runtime field; `custom_type_name` is the readable project/editor metadata.
    * [1] Editor changes
        * The only currently known value is the hash of "Spine". Hard-code migration.
        * Update Spine migration tests.
        * Update `save_data_project` content and tests.
    * [1] Bob changes
        * Update Bob to read `string` and build `int` hash.