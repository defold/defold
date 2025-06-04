# Defold Editor Systems Overview

## Startup

The Defold editor runs on the Java Virtual Machine. We bundle our own JVM with the editor and use a simple launcher executable to boot the JVM with a particular set of command-line arguments. The entry point is in `editor/src/java/com/defold/editor/Main.java`, which starts up a JavaFx `Application` subclass. A splash screen is displayed while a custom `ClassLoader` loads all the classes required to show the Welcome dialog. While it is shown, the custom `ClassLoader` keeps loading the classes required by the editor on all available background threads while the user ponders which project to open. Once that happens we await loading of all the remaining classes, then proceed to loading the project from disk.

We load the entire set of editable project data into memory, but non-editable resources such as images can be loaded on demand. From this, we create the project graph, which represents the complete state of all editable resources in the project.

## Graphs, nodes and connections

The Defold editor uses a graph-based data model. A graph consists of nodes and connections between these nodes. Nodes can have inputs, outputs and properties. Inputs can be connected to outputs or properties on other nodes. Properties are used to store state in a node, whereas outputs are pure functions that may depend on other outputs, inputs and properties from the same node. Node types are declared using the `g/defnode` macro, and can inherit from one or more node types like in an object-oriented programming language.

When a node output is evaluated, the connections are followed upstream until all the dependencies are satisfied in a recursive manner. For example, the `:scene` output of a `ShapeNode` might depend on a `:texture-image` input in the same node that is connected to an `:image` output on a `PngImageNode`. In this case, evaluating `:scene` on the `ShapeNode` means that we'd first have to evaluate `:image` on the `PngImageNode`.

Doing all this evaluation can take time, so the editor makes extensive use of caching. The value produced from any output marked as `:cached` will be stored in a cache for fast retrieval the next time it is evaluated. Entries will be evicted from the cache if any of their upstream dependencies change.

The inputs and outputs are generally constrained to a particular data type, but any output can also produce a special type of value called an `ErrorValue`. Error values will be propagated and in a sense "infect" anything downstream of them in the graph. As the error propagates, its path in the graph is recorded until it eventually reaches an input that is able to handle it by specifying a `:substitute` function. Typically the error value reaches a some sort of view where the error can be presented in a meaningful way and allow the user to navigate to the source of the error. Beware that evaluating an output programmatically may return an unexpected `ErrorValue` as a result of a upstream connection in the graph.

## Transactions

The graph is modified by executing transactions using the `g/transact` function. It takes a (possibly nested) sequence of `transaction-steps` and ensures all of them or none of them are applied. Each `transaction-step` only performs a small operation like "Create Node", "Set Property" or "Add Connection". Larger operations like adding a component to a game object are composed of many small `transaction-steps`. It is possible to evaluate the graph on a background thread by obtaining an `evaluation-context`, which contains a snapshot of the graph state, and supplying it with all graph queries. However, running `g/transact` from a background thread is not allowed, so a background thread must post `g/transact` calls to the main thread using something like `ui/run-later`.

## Multiple graphs

There might be several graphs in play at any one time. Typically, all the project data (i.e. "model" data) resides in one graph, whereas any number of views (as in user-interface elements) can have their own graphs whose node inputs are connected to the project graph. Closing a view discards the view graph but leaves the project graph intact.

The project graph has history enabled, which means that it keeps an ever-growing list of graph states that we append to every time an undoable action is performed. Undoing and redoing becomes a simple matter of pointing to one of the previous graph states. However, care must be taken when introducing state as any programmatic change to the project graph creates an undo step.

## The workspace

Alongside the project graph, there is also the workspace graph. The workspace is associated with the project directory on disk, and keeps track of files in the project. It does not host any of the editable state derived from those files - that is all in the project graph - rather, it keeps track of changes to the file system and is responsible for notifying the project graph of external modifications to the project files, etc. It also acts as a registry of the various `resource-types` (effectively based on file extensions) and the recipes for loading each type of resource into the project graph.

## Resource sync

Whenever an operation may have modified the files in the project folder, we trigger what is known as a `resource-sync`. During a `resource-sync`, the workspace figures out what has happened and updates the project graph to reflect the updated state. Files may have been added or deleted, moved to new locations, or changed their contents entirely. In response to this we might re-create parts of the project graph related to the modified resources from scratch.

Every file in the project has a corresponding `ResourceNode` in the project graph. We create new `ResourceNodes` in the project graph for all new or modified `Resources`, based on the `:node-type` associated with the `resource-type` registered for the file extension. We then use the `:read-fn` associated with the `resource-type` to read the contents of the `Resource` into its `save-value` representation.

It is possible to register fully custom `resource-types`, but for most cases you would use one of the helper functions to register either a Protobuf-based, `ini`-style settings-based, or code-style text-based `resource-type`.

The `save-value` representation varies by `resource-type`, but commonly, Protobuf-based resources are read as Clojure maps, code resources as read as a vector of line strings, and so on. The `:node-type` associated with the `resource-type` is expected to have a `save-value` output that produces a new `save-value` that includes any edits to its data.

When the project is saved, we compare the value produced by the `save-value` output to the value that was returned by the `:read-fn` at load time. If they differ, the file is considered dirty, and we will feed the updated `save-value` into the `:write-fn` associated with the `resource-type` to obtain a textual representation of the edited data, and write it to disk.

As you can gather, it is imperative that the value produced by the `:read-fn` exactly matches the value produced by the `save-value` output. If they do not, you will appear to have unsaved changes immediately after loading the project. To complicate matters, our project file formats have changed over time as new features were introduced. For Protobuf-based resources, this data migration is handled by the `:sanitize-fn` associated with the resource-type. If supplied, the `:sanitize-fn` will wrap the raw Protobuf `:read-fn` so that any silent data migrations can be applied to the read Clojure map before it is passed on to other systems. The `:sanitize-fn` should take the unmigrated Clojure map read from disk, and return a migrated Clojure map in the migrated format. The result is that our `:read-fn` will always return the latest data format.

Specifically for Protobuf-based `resource-types`, you can specify whether or not the Clojure map returned by the `:read-fn` should include default values for fields marked `optional`. In previous versions of Defold, all `optional` field values would be present in the returned map, and consistently saved in the project files. This lead to the project files containing a lot of redundant information. Nowadays, we strip default values from the Protobuf-based `resource-types` included in the editor and any editor extensions we develop ourselves. There are however situations where it is convenient for the `:read-fn` to return a map that includes defaults for all `optional` fields. If that is desired, you can specify `:read-defaults false` when registering your Protobuf-based `resource-type`.

If your Protobuf-based `resource-type` reads defaults, the `save-value` output of its associated `:node-type` must include defaults, and conversely exclude defaults if it does not. To help with this, you should use `protobuf/make-map-with-defaults` or `protobuf/make-map-without-defaults` respectively in your `save-value` output. You must also ensure your `:load-fn` produces a fully-formed graph node in the absence of `optional` field values, typically by declaring `(default (protobuf/default PbClass :pb-field))` for any node `properties` backed by `optional` Protobuf fields. The node property default must match the Protobuf backing field default, because the property will not be written to disk when it is set to the default value, so your `:load-fn` will never see it.

Back to the loading procedure, we have a newly created node in the project graph for each `Resource` in the project, but nothing has been loaded into the graph yet, and no connections have been established. Before we can start loading data into the graph, we must establish a load order. Certain `Resources` may reference data inside other `Resources`, and we want the referenced `Resources` to be loaded before the referencing `Resources`.

This load order is established by the `:dependencies-fn` associated with the `resource-type`. For Protobuf-based resources, the default `:dependencies-fn` finds references to other `Resources` by doing a recursive search for fields tagged `[(resource) = true]`, but some `Resources` that embed internal `Resources` as strings have specific implementations of the `:dependencies-fn`. The `:dependencies-fn` takes the `save-value` representation returned by the `:read-fn` and returns a vector of distinct `proj-path` strings that are referenced by the `Resource`. The editor collects this information from all the `Resources` we're about to load to produce a sorted list where all the referenced `Resources` precede their referencing `Resources`.

Once the correct load order has been determined, we load the data into the graph. For each node in order, we run the `:load-fn` associated with its `resource-type`, supplying it the `node-id` of the `Project` node, the `node-id` of the `ResourceNode` we're loading, the `Resource` the data is read from, and the `save-value` representation returned by the `:read-fn`. The `:load-fn` should return a sequence of `transaction-steps` that load the node into the graph. These `transaction-steps` can do various things, like set properties, establish connections, create sub-nodes owned by the `ResourceNode` we're loading, and so on.

In addition to the `transaction-steps` returned by the `:load-fn`, the editor will add a `transaction-step` to connect the populated node to the `save-data` input on the `Project` node if it represents a file in the project and provides a `:write-fn`.

It is important to understand that the `:load-fn` does not perform these changes to the project graph directly. Rather, it returns a sequence of `transaction-steps` that will be applied alongside the changes from other `Resources` that are loaded at the same time. The editor will also include `transaction-steps` from other file system changes such as deleted or renamed files.

When a project `Resource` has been externally modified, the loading process is performed on a new replacement `ResourceNode` that is separate from the existing `ResourceNode` that used to represent the `Resource`. We then update any existing connections to the old `ResourceNode` to instead refer to its replacement. Finally, we delete the old `ResourceNode` and any nodes connected to a `:cascade-delete`-marked input, recursively.

When a `Resource` is moved or renamed, the editor adds `transaction-steps` to set the `resource` property on the corresponding `ResourceNode` to the updated `Resource` location. This ensures any references to the path will be updated in the project files, as long as they use the value from the `resource` property.

Deleted `Resources` are handled in a different fashion. Here the old `ResourceNode` remains in the graph along with all its connections to other nodes, but the `ResourceNode` is marked as defective. In this state, its outputs are jammed with an `ErrorValue` denoting the file as missing. Anything that depends on the deleted `Resource` will produce an `ErrorValue` with information about the missing `Resource`. If the deleted `Resource` reappears on disk, the defective `ResourceNode` will be replaced with a non-defective one during `resource-sync`.

At present, the `node-ids` of the recreated substructure will not match up to the `node-ids` from the old, deleted `ResourceNodes`. Objects might have been removed or added in a different order to the modified file, so it is difficult to retain the structure. Because of this, we must clear the undo history whenever we perform these changes during a `resource-sync`.

Once the editor has collected all the `transaction-steps` that describe the changes to the project graph, they are executed in an isolated transaction. To the outside world, it appears as if all the loaded nodes were suddenly populated and connected to the project graph in one go.

## View types

Along with resource types, view types are also registered with the workspace. Current examples of view types include a code editor used for textual editing and a scene view used to arrange visual elements. The same scene view component is used to edit collections, tile maps and texture atlases. It is also used to preview assets such as meshes and cube map textures.

Every resource type can support editing through one or more view types, specified when registering the resource type. The actual contract required to edit a resource using a particular view type is loosely defined by an expected set of properties and outputs on the edited `ResourceNode`. The `:make-view-fn` provided when registering a view type is responsible for connecting the view node to the edited resource. For example, a resource can be opened in the scene view if it includes the `:scene` type among its supported `:view-types` when the resource type is registered. When the view is created, the `:make-view-fn` connects the `:scene` output of the `ResourceNode` to the `:scene` input on the `SceneView` node. The scene is described as data, and the `ResourceNode` simply needs to provide the data required to render its scene.

## Scene view scenes

A scene is a map that typically contains a local transform, a local-space axis-aligned bounding box, a renderable, and optionally a vector of child scenes in the same vein. Each renderable specifies a render function and some user data that will be supplied to the render function along with render-args that include the world-space matrices and so on. Rendering is split into passes so that transparent elements can be blended correctly onto opaque elements, among other things. The renderables specify which passes it should be rendered to.

When rendered, the scene hierarchy is flattened so that each renderable obtains a world-space transform. The axis-aligned bounding boxes of parent scenes are expanded to include the transformed bounding boxes of child scenes. Similar renderables are batched together, and the render function is called with all the similar renderables in one go. Renderables can specify a `:batch-key` to control which renderables are batched together.

## The scene cache

Frequently there is a need to create an object that is managed by the OpenGL context. This can be done directly in the render function call if you've previously registered an object type with the scene cache system. You provide functions for creating, updating and destroying the OpenGL-managed objects, and can then request an object from the scene cache from directly within a render function. The scene cache will create an instance of the requested object if it had not been requested before, return the existing one if the parameters match, or update the existing one if they differ. If the render function does not request the object during a render call, it is pruned from the cache automatically. This way, users do not have to worry about explicitly creating or destroying OpenGL resources in response to scene changes.

## Save data

Any `ResourceNode` that connects to the `:save-data` array input of the project node may be saved when the user invokes the Save command. Each save data entry is a map with a `:node-id`, `:resource`, `:save-value`, and `:dirty` fields. Saving will write the serialized form of the `:save-value` to disk for every save data entry that is marked `:dirty`. Every savable `resource-type` specifies a `:write-fn` that takes the `:save-value` and returns a string that is then written to the `:resource`.

Unless explicitly prevented from doing so, all resource nodes that correspond to editable project files will be automatically connected to the `:save-data` input of the project node when loaded, as long as their resource-type specifies a `:write-fn`. 

The resource nodes are not supposed to implement the `:save-data` output themselves. Instead, they implement the `:save-value` output. The base `ResourceNode` `:save-data` output takes the `:save-value` and compares it against the value that was loaded using the `resource-types` `:read-fn`, and returns a populated save data entry with the `:dirty` flag set accordingly.

## Build targets

In order to run the game, the project resources must be compiled into a binary format for the engine runtime. In the graph, the "recipe" for such a binary output file is called a build target.

Build targets are maps containing all the necessary information to produce a binary from a particular resource that can be loaded by the engine runtime. Build targets specify an output build `:resource` (i.e. an output file below the build output folder for the resulting binary), a `:build-fn` that will be called to produce the resulting binary, and `:user-data` that will be given to the `:build-fn` as an argument. The build function is expected to return a map with the above build `:resource` and a `:content` field. The content is an array of bytes that should be written to disk at the location denoted by the build resource. The contents of the `:user-data` is hashed along with the build function and later used as a key in a disk-based build cache to speed up subsequent builds.

A build target can optionally specify a list of `:deps` - build targets that it depends on. The build functions of all dependent build targets will be run before the depending build target is built. The build system will also attempt to fuse equivalent build targets into a single build output file if possible in order to save memory in the running game. Each build function must update any internal references to the fused build resources from its dependent build targets by looking up the original resource in the `dep-resources` mapping provided to the build function as an argument.
