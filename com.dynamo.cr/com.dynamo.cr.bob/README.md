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
host's LuaJIT executable, Ogg libraries, and DXC compiler library. CMake stages
DXC alongside the shader compiler before building Bob Light, without requiring
an engine install first.
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

DX12 is still a work in progress. When switching to the SM6/DXC shader compiler,
clear the project's build caches and rebuild its content.

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

## Archive manifests

Set `archive.manifest = /content.archivemanifest` in `game.project` to use named
packages:

```ini
[archive]
manifest = /content.archivemanifest
```

Explicitly setting both `archive.manifest` and `liveupdate.settings` is an error
for both build and bundle. Clear one of the settings. The inherited default
`liveupdate.settings` path does not count as an explicit selection.

The `liveupdate.settings` workflow is deprecated but remains supported. Use
`archive.manifest` for new projects. Without an archive manifest, legacy resource
selection is preserved:
a resource reachable through any non-excluded route remains bundled. Bob adapts
the final legacy base/Live Update sets to one internal package and uses the same
preparation and writers as manifest projects. It does not generate rules or change
project files. In legacy mode, ZIP/folder destinations and compression still come
from the selected `.settings` file; package manifests contain only their packaged resources.
Legacy folder outputs retain previous resources and unrelated files, as before.
Existing legacy ZIP/folder outputs in a bundle can be replaced without an output index.
The [example project](../../editor/test/resources/live_update_packages_project)
separates common content, two episode ranges, and seasonal content:

```protobuf
output_directory: "build/packages"
base_file_compression_preset: "lz4"
container_presets { name: "zip" zip_level: 9 }
container_presets { name: "folder" provider: "folder" }
file_compression_presets { name: "lz4" lz4_level: 12 }
file_compression_presets { name: "none" file_compression: NONE }
packages {
  name: "common"
  container_preset: "zip"
  file_compression_preset: "lz4"
  rules { name: "Common content" path: "/common/.*" }
}
packages {
  name: "episodes_common"
  rules { name: "Shared episode content" path: "/episodes/common/.*" }
}
packages {
  name: "episodes_00_09"
  rules { name: "Episodes 00–09" path: "/episodes/0[0-9]/.*[.](collection|txt)" }
  rules { name: "Separate shared content" operation: REMOVE package: "episodes_common" }
}
packages {
  name: "episodes_10_19"
  rules { name: "Episodes 10–19" path: "/episodes/1[0-9]/.*[.](collection|txt)" }
  rules { name: "Separate shared content" operation: REMOVE package: "episodes_common" }
}
packages {
  name: "halloween"
  rules { name: "Halloween content" path: "/halloween/.*[.](collection|txt)" }
}
```

The manifest stores independent container and file-compression presets. Packages
select `container_preset` (ZIP, Folder, or a custom provider) and
`file_compression_preset` (LZ4 or None). Base selects only
`base_file_compression_preset`.

ZIP presets configure `zip_level` from `-1` to `9` (default `1`; `-1` uses the
Java default, `0` disables compression). LZ4 presets configure `lz4_level` from
`1` to `12` (default `9`). Folder and None have no compression-level setting.
Omitted selections use ZIP and LZ4 defaults. Unknown references, duplicate names
within either preset group, and invalid levels are errors.

In the editor, configure these under Compression Types → Container or File.
A Folder package using None stores files without LZ4 compression. Manifest mode
replaces `project.compress_archive`; legacy projects continue using that setting.
Conversion copies legacy container and file settings into separate presets.

A resource shared by packages with different file compression gets the matching
payload and hash in each package's manifest. LZ4 retains the existing threshold:
files that do not compress enough, and compiler outputs marked uncompressed, are
stored unchanged.

Rules run in order. Java regular expressions match the entire source path,
including its leading slash. Selected sources contribute their actual compiled
outputs and runtime dependencies; unrecognized source files are copied unchanged.
A package reference uses the referenced package's final contents, including
forward references. Reference cycles, missing packages, invalid regexes, and
missing dependencies are errors. Each Path rule has a **No matches** policy:
**Warn** (the default), **Error** (fail the build), or **Ignore**. In protobuf text,
set `no_match: WARN`, `no_match: ERROR`, or `no_match: IGNORE` on the rule.
This applies to Add and Remove rules and only controls a valid pattern matching
no source files. It does not suppress invalid regexes or missing dependencies.
Regex errors identify the package, rule, and 1-based column in the Path expression.
Resources may belong to several packages. Startup-required resources remain in
the base even when selected by a package. Proxies are deferred boundaries;
factory loading modes determine which prototypes must remain available at startup.

The episode rules select collections and their dependencies, plus custom `.txt`
files. Each Remove rule subtracts the complete `episodes_common` package after
the Add rule. Dependencies shared with `common` can still occur in several
packages; the editor's resource search lists all of them.

Custom files, such as `.txt` and `.json`, keep their original paths and contents.
Select them with a Path rule; adding them to `project.custom_resources` is not
required. Once their package is mounted, read them with `sys.load_resource()`,
for example `sys.load_resource("/episodes/00/description.txt")`.

`build` compiles resources and saves their dependency graph in `game.graph.json`.
`bundle` reads that graph and the prebuilt resources, then applies the current
archive manifest. It never compiles missing or changed sources. Use the same
platform, architectures, variant, and compiler settings for both commands.

```sh
java -jar bob.jar build
java -jar bob.jar --all-packages --bundle-output bundle bundle
```

Without selection options, bundle produces a self-contained Base containing all
selected content. `--all-packages` writes Base and every external package.
`--package base` writes only the manifest's Base, preserving its startup locks.
Repeat `--package` to select external packages without producing an application:

```sh
java -jar bob.jar --package common --package halloween build
java -jar bob.jar --package common --package halloween bundle
```

The first selective build establishes a complete graph automatically. Later
selective builds compile the selected roots and their dependency closure, retaining
other compiled results. New roots or incompatible settings establish the complete
graph again using the ordinary compiler cache. Every bundle recreates the selected
archives and preserves unselected package outputs.

Current manifest rules, compression, provider settings, and output paths take effect
at bundle time. Rules selecting resources absent from the saved graph require another
build. `--all-packages` and `--package` cannot be combined; `--package` also cannot be
combined with `--build-input` or `--build-input-file`.

`--archive` and `-a` are accepted with a deprecation warning and ignored.
`--liveupdate yes` is a deprecated alias for `--all-packages`; `no` keeps the default.
Explicit new selection options take precedence over the deprecated flag.
Legacy settings expose their external content as the package `liveupdate`.

The `zip` provider is the default and writes `<name>.zip`; `folder` writes `<name>/`. The manifest's `output_directory`
sets the parent directory for every provider and defaults to `build/packages`.
Relative paths resolve from the project root; absolute paths are also accepted.
Environment variables use `${NAME}`, `$NAME`, or `%NAME%`, for example
`output_directory: "${PACKAGE_OUTPUT}/releases"`. Bob resolves these when exporting
and reports an error for undefined variables. The current values determine
the output paths on every export.

ZIP compression defaults to `1` and can be set in the selected container preset. Manifest mode uses its own output directory and provider
settings; it does not read a separate legacy `.settings` file.

Native engine-only bundles (`--exclude-archive`) skip package copying and require
no package output index.

Each package contains a `liveupdate.game.dmanifest` with only that package's
resource entries. Optional metadata identifies outputs and external requirements,
including alternative packages supplying the same dependency. It is informational:
the game is responsible for downloading, choosing mount order, and mounting its
packages with the existing Live Update API.

### Package reports

Use `--bundle-report-html` with `bundle` to inspect the base and all Live Update
packages selected by that invocation in one standalone file. It also supports
legacy projects:

```sh
java -jar bob.jar --all-packages --bundle-report-html bundle-report.html build bundle
```

The report lists package providers, resource counts, compiled sizes, and the
actual sizes and paths of generated ZIPs, folders, and custom outputs. Folder
sizes include all files, including the package manifest. Select a package to
inspect its contents, or search across all packages. Search is case-insensitive
and matches nonconsecutive characters in resource and source paths; for example,
`ep00desc` matches `/episodes/00/description.txt`. Shared resources list every
package containing them. JavaScript, styles, and report data are embedded, so
the HTML works offline without a server or companion files.

Bob saves package contents in `game.lureport.json` alongside the build outputs.
The report is written after every requested bundle succeeds. A later invocation
can use `--all-packages --bundle-report-html bundle-report.html bundle` to regenerate
packages and the report from the saved build graph without compiling resources.

`--build-report-html` and `--build-report-json` remain options for the `build`
command. They run after compiling, independently of bundling; `bundle` alone
does not generate or overwrite build reports. Build reports show compiled byte sizes;
archive compression and container sizes belong to the bundle report.

### Package builds and bundle placement

Every `bundle` invocation recreates the selected archives from scratch.
Bob uses its normal build cache for resource compilation during `build`. Each provider
writes to an empty staging directory, and Bob installs the completed outputs
only after every provider succeeds. Package generation does not use cache keys
or skip unchanged packages.

The `game.lupackages` file lists the artifacts generated by the latest `bundle` invocation.
Output records in `.internal/liveupdate` retain generated paths across clean
builds and disabled export, keeping generated packages out of resource matching.
Renamed or removed artifacts are retired after all selected providers complete successfully.
Explicit package selection preserves artifacts owned by unselected packages.
Moving or copying a project rebases ownership of artifacts within the project;
it does not transfer ownership of external output directories.

Manifest packages are written only to the manifest's output directory. Legacy
`save-zip-in-bundle-folder` and `save-folder-in-bundle-folder` settings request an
additional copy during `bundle`. Bob copies these artifacts through fresh staging and
removes obsolete owned outputs. Missing source artifacts or invalid file types
require another `bundle`. Failed or cancelled delivery restores
the previous package copies and output records, so the bundle step can be retried.
Source artifacts remain at their configured destinations.

### Java providers

Implement `com.dynamo.bob.archive.liveupdate.PackageProvider` in the existing
`com.defold.extension.pipeline` plugin namespace. `getId()` identifies the
provider in a container preset. `describeOutputs()` declares relative artifact names
and whether each is a directory. Bob checks collisions before calling `write()`.

Override `getDisplayName()` and `getProperties()` to expose settings on the
manifest's top node. Each ordered `Property` declares an ID, label, type
(`STRING`, `BOOLEAN`, `INTEGER`, or `NUMBER`), a text default, and optional text
choices. Bob validates descriptions and parses values; the editor uses the same
schema for controls and validation. Settings apply to every package using that
provider, and are saved independently of package membership:

```protobuf
providers {
  provider: "studio-package"
  properties { name: "quality" value: "9" }
}
```

Both output description and writing receive typed, immutable values through
`context.providerSettings().get(getId())`. Settings for unused providers remain
in the authored manifest but are not applied. Built-in ZIP/Folder compression is
configured through presets. Providers can inspect the manifest and its presets
through `context.manifest()`.

`write()` receives an immutable context and package plan, prepared resource
streams, a package-local manifest, and a staging directory. Resource streams
already contain the engine's 16-byte Live Update header and the prepared
compressed/encrypted payload. Use each resource's hash as its content identifier.
The context includes base membership, source/output mappings, runtime dependencies,
provenance, sizes/hashes, platform, architectures, variant, and effective settings.
Both callbacks share the same context, whose resource hashes and sizes use Base
compression. Use `PreparedResource.info()` for each package's actual payload
hashes and sizes. Declaring outputs does not prepare package compression variants.
Argumentless CLI options may have null values. Providers cannot alter membership.
Poll `context.checkCanceled()` during long operations and close opened streams.
Write only inside the assigned staging directory. Bob publishes declared outputs
only after all providers succeed, and cleans staging on failure or cancellation.

### Editor preview

Open `.archivemanifest` to edit packages and ordered rules in the outline/properties.
Select the manifest in the outline to edit **Output Directory** and settings
for each provider used by its packages. The output directory has a folder picker. Each package shows its resolved artifact paths; right-click
a path to copy it or open its folder. Before export, **Open Folder** opens the
nearest existing parent directory.
Use the outline context menu to add, rename, delete, or reorder packages and rules.
Package targets use a dropdown that excludes the current package and any package
that would create a reference cycle. Bob supplies these choices and validates
references for both Add and Remove rules, including manifests edited as text.
The preview resolves the editor's dependency graph on a background thread,
including unsaved edits. Selecting a resource highlights its contributing rules
and exposes its source and dependency path. Exact compiled contents and sizes
for the bundle target are available in the bundle report.

Changes update the preview automatically after a short debounce. Closing the
view stops active preview work; obsolete completions cannot replace a newer
result. The editor previews package membership; Bob builds and packages the
selected resources. Bundling always resolves and validates the manifest again.
Proxy `Exclude` values remain serialized, but the manifest
controls packaging when selected. Selecting a legacy `.settings` file in
`game.project` restores proxy `Exclude` resource selection and legacy publisher settings.
Opening a bundle dialog does not migrate or change these settings.
