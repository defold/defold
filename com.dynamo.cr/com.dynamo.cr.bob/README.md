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

### Byte order

When reading or writing binary graphics resources in Java, use
`ByteOrder.LITTLE_ENDIAN` where the resource format requires little-endian
values. Do not use the host's native byte order for those fields.

### Updating the build report template

The [build report template](lib/report_template.html) includes its JavaScript
and CSS inline so Bob can serve one HTML file. When updating those libraries,
run `inline_libraries.py` from `share/report_libs/` and insert its generated
`<style>` and `<script>` tags. See the [report library guide](../../share/report_libs/README.md).

## Live Update package manifests

Set `liveupdate.manifest = /content.lumanifest` in `game.project` to use named
packages. Leaving this setting empty preserves the legacy proxy/publisher
workflow. The [example project](../../editor/test/resources/live_update_packages_project)
separates common content, two episode ranges, and seasonal content:

```protobuf
output_directory: "build/packages"
packages {
  name: "common"
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

Rules run in order. Java regular expressions match the entire source path,
including its leading slash. Selected sources contribute their actual compiled
outputs and runtime dependencies; unrecognized source files are copied unchanged.
A package reference uses the referenced package's final contents, including
forward references. Reference cycles, missing packages, invalid regexes, and
missing dependencies are errors. An expression matching no files is a warning.
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

Use `--archive --liveupdate yes` to export. With Live Update disabled, the selected
content is included in the base instead. The `zip` provider is the default and
writes `<name>.zip`; `folder` writes `<name>/`. The manifest's `output_directory`
sets the parent directory for every provider and defaults to `build/packages`.
Relative paths resolve from the project root; absolute paths are also accepted.
Environment variables use `${NAME}`, `$NAME`, or `%NAME%`, for example
`output_directory: "${PACKAGE_OUTPUT}/releases"`. Bob resolves these when exporting
and reports an error for undefined variables. Changing their values also
invalidates incremental package placement.

ZIP compression can be set in the manifest (see provider properties below).
Until overridden there, Bob uses `zip-compression-level` from `liveupdate.settings`,
or `1` if absent. Bundle-copy and Amazon settings remain in `liveupdate.settings`.
Its `zip-filepath`, `output-directory`, and single archive/folder name settings
are ignored in manifest mode.

With `mode = Amazon` in `liveupdate.settings`, package providers still generate
local artifacts. Bob uploads the complete artifact set only after every requested
bundle finishes successfully. ZIP and custom files use their declared names
under `amazon-prefix`; folder artifacts retain their relative directory layout.
The existing Amazon credential profile, bucket, and prefix settings apply.
Build-only and preview commands do not upload packages. A failed or cancelled
build or bundle never starts publication.

To publish without bundling, or retry a failed upload without rebuilding, run
`bob --liveupdate yes publish` with the same project and build output directory.
The `publish` command runs after all other requested commands succeed. Bob checks
the complete set against `game.lupackages` before contacting Amazon; missing or
modified artifacts require a build first. Publishing uploads the full set again
and does not delete remote objects. An interrupted upload can leave a partial
remote set; retry publishing to complete it. Projects without a manifest retain
the legacy resource publishing behavior.

Each package contains a `liveupdate.game.dmanifest` with only that package's
resource entries. Optional metadata identifies outputs and external requirements,
including alternative packages supplying the same dependency. It is informational:
the game is responsible for downloading, choosing mount order, and mounting its
packages with the existing Live Update API.

### Incremental packages

Package generation and bundle placement are incremental. Bob uses its existing
task signatures in `_BobBuildState_` together with artifact ownership receipts in
`.internal/liveupdate`. The resource graph supplies dependencies; the receipts
additionally record provider inputs and artifact content hashes. The build's
`game.lupackages` file lists the artifacts to place in the current bundle.

When a build changes one package, unchanged ZIP and folder packages are reused.
Changes to membership, payloads, manifests, or relevant provider settings rebuild
the affected packages. Missing or modified output artifacts are regenerated even
when source files have not changed. Ownership survives clean builds and disabling
export, so existing outputs never become authoring inputs. Renamed or removed
artifacts are retired after all providers complete successfully.
Moving or copying a project rebases ownership of artifacts within the project;
it does not transfer ownership of external output directories.

During `bundle`, unchanged artifact copies are preserved. Missing or changed files
are copied, and obsolete owned artifacts are removed. Package destinations are
checked against application outputs before copying. Missing or
modified source artifacts require another `build` before bundling. HTML5 stages
its rebuilt application files and publishes them together with changed packages,
preserving unchanged package copies. Other platform bundlers still control
recreation of their application output; package copies are restored when that
output directory is recreated.
Failed or cancelled delivery restores the previous artifacts and ownership receipt,
so the bundle step can be retried.

### Java providers

Implement `com.dynamo.bob.archive.liveupdate.PackageProvider` in the existing
`com.defold.extension.pipeline` plugin namespace. `getId()` identifies the
provider in `.lumanifest`. `describeOutputs()` declares relative artifact names
and whether each is a directory. Bob checks collisions before calling `write()`.

Override `getDisplayName()` and `getProperties()` to expose settings on the
manifest's top node. Each ordered `Property` declares an ID, label, type
(`STRING`, `BOOLEAN`, `INTEGER`, or `NUMBER`), a text default, and optional text
choices. Bob validates descriptions and parses values; the editor uses the same
schema for controls and validation. Settings apply to every package using that
provider, and are saved independently of package membership:

```protobuf
providers {
  provider: "zip"
  properties { name: "compression-level" value: "9" }
}
```

Both output description and writing receive typed, immutable values through
`context.providerSettings().get(getId())`. Settings for unused providers remain
in the authored manifest but are not applied. Provider settings participate in
incremental output invalidation. ZIP exposes compression level (`0`–`9`, plus
`-1` for the ZIP library default); Folder has no additional settings.

`write()` receives an immutable context and package plan, prepared resource
streams, a package-local manifest, and a staging directory. Resource streams
already contain the engine's 16-byte Live Update header and the prepared
compressed/encrypted payload. Use each resource's hash as its content identifier.
The context includes base membership, source/output mappings, runtime dependencies,
provenance, sizes/hashes, platform, architectures, variant, and effective settings.
Argumentless CLI options may have null values. Providers cannot alter membership.
Poll `context.checkCanceled()` during long operations and close opened streams.
Write only inside the assigned staging directory. Bob publishes declared outputs
only after all providers succeed, and cleans staging on failure or cancellation.

Override `getCacheKey()` to opt in to artifact reuse. Return a stable, non-null key
for equivalent provider behavior and inputs. Bob fingerprints the provider class,
the complete context, and the package manifest and prepared payload identities.
Change the key when helper libraries or external inputs change. The default null
key calls `write()` whenever Bob reruns package resolution. `describeOutputs()` is
always called during resolution so output collisions are validated before reuse.

### Editor preview

Open `.lumanifest` to edit packages and ordered rules in the outline/properties.
Select the manifest in the outline to edit **Output Directory** and settings
for each provider used by its packages. The output directory has a folder picker. Each package shows its resolved artifact paths; right-click
a path to copy it or open its folder. Before export, **Open Folder** opens the
nearest existing parent directory.
Use the outline context menu to add, rename, delete, or reorder packages and rules.
Package targets use a dropdown that excludes the current package and any package
that would create a reference cycle. Bob supplies these choices and validates
references for both Add and Remove rules, including manifests edited as text.
The preview automatically captures unsaved content and compiles an isolated
snapshot through Bob without calling provider writers. It uses the most recently bundled
platform, architectures, variant, and texture-compression option; before the first
bundle it uses host/debug. Selecting a compiled resource highlights its contributing
rules and exposes its source and dependency path. The byte counts are compiled
resource sizes, not final container download sizes.

Changes update the preview automatically after a short debounce. Each open view
reuses an isolated snapshot and Bob's compiled-resource cache. Close the view to
stop active preview work and remove its cache; obsolete completions cannot
replace a newer result. Ordinary editor builds
also include manifest-selected resources. Bundling always resolves and validates
the manifest again. Proxy `Exclude` values remain serialized, but the manifest
controls packaging when selected. When opening a project without a configured
manifest, the editor migrates checked proxy `Exclude` values into exact collection
path rules in one package. Remove rules for unchecked proxy targets preserve
their content and shared dependencies in the base. This conservatively retains
their entire closures, including nested proxy content; the generated rules can
be refined afterward. It creates `liveupdate.lumanifest` (with a numeric suffix
if that path is occupied) and saves the `liveupdate.manifest` setting in
`game.project`. Proxy files remain unchanged. Existing manifests are not migrated.
The migration retains the ZIP or folder provider, defaulting to ZIP for other
modes. Amazon publishing settings remain in `liveupdate.settings` for the final
publishing phase. It keeps valid output names and uses `liveupdate` otherwise,
adding a numeric suffix when the package output already exists to preserve it,
and copies the legacy output directory and ZIP compression into the manifest. Bundle-copy
settings continue to apply. Bob alone retains the
legacy workflow until a manifest is selected.
