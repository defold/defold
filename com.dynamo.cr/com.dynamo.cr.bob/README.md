# Bob the builder

For local build and test commands, see the [Bob guide](../README.md).
For IntelliJ IDEA setup and debugging, see the [IDEA guide](../README_IDEA.md).

## Packaging

Bob and Bob Light package tools directly from `$DYNAMO_HOME/ext` and use the
engine helper JARs in `$DYNAMO_HOME/share/java` as compilation dependencies.
Run `./scripts/build.py install_ext` before building Bob or Bob Light. It installs
the tools packaged for all supported hosts, including LuaJIT. `build_bob` uses
those installed tools without extracting their packages again; for a fresh
installation, run `./scripts/build.py install_ext build_bob` from the repository root.

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
`archive-artifacts.json`.
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
