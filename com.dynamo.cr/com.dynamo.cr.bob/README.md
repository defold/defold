# Bob the builder

For local build and test commands, see the [Bob guide](../README.md).
For IntelliJ IDEA setup and debugging, see the [IDEA guide](../README_IDEA.md).

## Packaging

Bob and Bob Light package tools directly from `$DYNAMO_HOME/ext` and use the
engine helper JARs in `$DYNAMO_HOME/share/java` as compilation dependencies.
Run `./scripts/build.py install_ext` before building Bob or Bob Light. It installs
the prepackaged tools for all supported hosts and builds LuaJIT and
[Ogg tools](../../external/ogg/README.md) for the host and supported targets.
Full Bob also needs these tools for the other desktop hosts; install them with
`sync_archive` for an archived revision, or build them on their respective
platforms. `build_bob` uses the installed tools without extracting their packages
again.

Full Bob uses engines and builtins from `$DYNAMO_HOME/archive/<revision>/engine`
when available, otherwise from the local engine build. Compiler libraries and
helper JARs prefer local builds, with the archive as a fallback. Gradle reads
artifacts from their build or archive locations without staging them in the Bob
source tree's `lib` and `libexec` directories. Console shader compiler plugins,
where needed, are built separately from Bob.

`scripts/update-editor-binaries.sh` installs Bob with `-Pprefer-local-engines` so
local engine binaries and `classes.dex` take precedence over a synced archive.
Platforms without a local engine still use the archive. The local preference also
includes available headless engines and local platforms omitted from
`archive-artifacts.json`.

`./scripts/build.py sync_archive` downloads Bob's engine inputs from S3. The public
paths in `archive-artifacts.json` are shared with Gradle's artifact selection;
update that list when adding an archived input. Private-platform archive folders
retain their existing download filters.

Tool packaging is defined by two additional manifests:

- `tools.json` maps full Bob's JAR entry paths to installed files relative to
  `$DYNAMO_HOME`. It includes external tools and their supporting libraries/data.
- `bob-light-tools.json` lists Ant-style JAR path patterns for Bob Light's tools,
  compiler libraries, and LuaJIT modules. Android bundling tools such as `aapt2`
  are included only in full Bob.

Compiler libraries resolve from local engine builds or archive paths listed in
`archive-artifacts.json`. `sync_archive` installs archived LuaJIT and Ogg tools
into their usual `$DYNAMO_HOME/ext/bin/<platform>` and `ext/lib/<platform>` paths,
preserving locally built tools for the current host and selected target.
Full Bob requires them for all desktop hosts; Bob Light requires the current
host's LuaJIT executable and Ogg libraries.
The LuaJIT modules installed in `$DYNAMO_HOME/ext` are assembled into
`tmp/luajit-share.zip` and packaged inside each JAR as `lib/luajit-share.zip`.
Both JAR tasks track the manifests as inputs, so changing a list updates the
packaged contents.

Bob's game archive compression uses the engine's raw LZ4 high compression
implementation through JNA and `dlib_shared`, packaged in both Bob and Bob Light.
Local builds must include `dlib_shared` for the host platform.

Both JARs copy dependency entries directly when their compression method matches
the output, preserving the compressed bytes. Loose files are compressed in
parallel. `-Pkeep-bob-uncompressed` still stores every entry without compression;
compressed dependency entries are inflated once while writing the output.

## Android resource shrinking

Selecting `android.r8_keep_rules` enables R8 code optimization and, with SDKs that
support it, Android resource shrinking. Extender runs both in the same R8 invocation
and returns the optimized proto archive as `compiledresources.apk` for Bob to package
into APK and AAB files.

Resources accessed only through names constructed at runtime or native code need
an explicit resource keep rule. For example, with `project.bundle_resources` set to
`/bundle`, put this in `bundle/android/res/raw/keep.xml`:

```xml
<resources xmlns:tools="http://schemas.android.com/tools"
    tools:keep="@drawable/dynamic_icon,@raw/runtime_data" />
```

Resource keep XML is separate from the Java `.keep` configuration. Projects without
`android.r8_keep_rules` continue using D8.

## Content pipeline

Bob builds content for the editor and engine tests. `build_engine` builds host
tools and `bob-light.jar` before CMake uses Bob Light to compile engine test
content and built-in resources.

### GUI custom node callbacks

`IGuiCustomNode` has optional `migrateScene(GuiSceneMigrationContext)`,
`create(GuiBuildContext, Task.TaskBuilder<?>, NodeDesc, String layoutName)` and
`build(GuiBuildContext, Task, NodeDesc.Builder, String layoutName)` callbacks.
Property registration remains static, and `addProperty` is unchanged. Plugins
implementing any callback need a public no-argument constructor. Bob creates one
instance per custom type and GUI task, shared by source migration and that
task's nodes and layouts.

Use `migrateScene` to convert legacy source formats into ordinary custom nodes
and generic resources. Its context provides the source GUI and mutable
`getSceneBuilder()`. Bob calls registered migrations in type-name order, once
per source file in the task, including templates, before dependency discovery,
defaults and inheritance. Migrations run even when the source contains no nodes
of the registered type. They must modify only their own formats and must not
read external files. Keep sparse overrides sparse, preserve explicit empty or
false overrides, and leave validation to `create`/`build`. Include a migration
version with `context.addExtraCacheKey(...)` to invalidate cached output when
the conversion rules change. Bob retains the migrated source and uses clones
for each template instance.

Bob creates one `GuiBuildContext` per GUI task and shares it across all custom
types, nodes, layouts and both callback phases. It holds the project, source GUI
and resource map. The task, node and layout name are passed as callback arguments.
`create` receives an immutable node; `build` receives the output node builder
directly, without allocating a context or node snapshot for each callback.

Both callbacks receive the node after defaults, template inheritance and layout
overrides have been resolved and Bob has checked custom property types against
their registration. Plugins only need to validate domain rules, such as whether
an animation exists. Template children have their instance prefix in
the node ID. The default layout name is an empty string.
`GuiBuildContext.getProperty(node, name)` looks up a resolved custom property on
either a node or its builder. `context.getResources()` maps generic GUI resource
names to their original source files, including inherited resources.
Local resource names take precedence over resources from templates.
The same resolution pass selects original paths for callbacks and compiled
paths for the output, including collisions between nested or sibling templates.
Templates are source dependencies of the containing GUI. Bob validates their
custom nodes after applying the containing GUI's overrides, so a parent can
provide a required value left empty in a template. Building the template as a
standalone GUI still validates its own values.

Use `create` to declare every dependency read by the plugin. Add source files
with `taskBuilder.addInput(resource)`. For compiled dependencies, use
`context.getProject().createTask(resource)` and pass the resulting task to
`taskBuilder.addInputsFromOutputs(task)`. Compiled outputs are available
in `build`, after the dependency tasks finish. Declare underlying data files
explicitly if changes to them can affect validation without changing a compiled
descriptor. For example, Spine animation validation needs the skeleton data as
an input, even when the scene descriptor stays unchanged.

Use `build` to validate values and modify the supplied node builder. The node ID and
custom type must remain unchanged. Added custom properties can use string IDs;
Bob converts them to hashes before serialization. Throw `context.error(node, layoutName, message)`
to report a `CompileExceptionError` identifying the GUI, node and layout. For
example, a plugin can enforce a nonempty selection without property flags:

```java
@Override
public void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) throws CompileExceptionError {
    if (GuiBuildContext.getProperty(node, "scene").getString().isEmpty()) {
        throw context.error(node, layoutName, "Scene must not be empty");
    }
}
```

`create` runs when Bob constructs tasks, including incremental builds. `build`
runs only when Bob rebuilds the GUI, not when it skips the task or restores a
cached output. Include a plugin version and any relevant settings using
`taskBuilder.addExtraCacheKey(...)` so changes in plugin behavior invalidate
cached output. Existing plugins that only register properties or provide the
legacy static `migrateProperties(Map)` hook do not need to implement callbacks
or expose a constructor. Legacy scene/node conversion now belongs to the
plugin's `migrateScene` implementation; Bob does not extract old plugin-specific
protobuf fields into that property map. Use an updated plugin when building
files that still contain its legacy format. Unsupported node types and protobuf
fields marked deprecated produce a build error if no plugin migrates them,
including sparse override markers that select an omitted default value. These
checks run before task signatures so cached output cannot hide a missing migration.

### Byte order

When reading or writing binary graphics resources in Java, use
`ByteOrder.LITTLE_ENDIAN` where the resource format requires little-endian
values. Do not use the host's native byte order for those fields.

### Updating the build report template

The [build report template](lib/report_template.html) includes its JavaScript
and CSS inline so Bob can serve one HTML file. When updating those libraries,
run `inline_libraries.py` from `share/report_libs/` and insert its generated
`<style>` and `<script>` tags. See the [report library guide](../../share/report_libs/README.md).
