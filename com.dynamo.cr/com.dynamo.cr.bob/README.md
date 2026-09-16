# Bob the builder

## Packaging

Bob and Bob Light package tools directly from `$DYNAMO_HOME/ext` and use the
engine helper JARs in `$DYNAMO_HOME/share/java` as compilation dependencies.
Run `scripts/build.py install_ext` before building Bob or Bob Light. It installs
the tools packaged for all supported hosts, including LuaJIT. `build_bob` uses
those installed tools without extracting their packages again; for a fresh
installation, run `scripts/build.py install_ext build_bob`.

Full Bob uses engines and builtins from `$DYNAMO_HOME/archive/<current revision>`
when available, otherwise from the local engine build. Compiler libraries and
helper JARs prefer local builds, with the archive as a fallback. Public artifacts
are no longer copied into Bob's `lib` and `libexec` directories. Private-platform
additions can still be provided by `scripts/copy_private.sh`.

`scripts/update-editor-binaries.sh` installs Bob with `-Pprefer-local-engines` so
local engine binaries and `classes.dex` take precedence over a synced archive.
Platforms without a local engine still use the archive. This also includes local
headless engines and platforms omitted from `archive-artifacts.json`.

`scripts/build.py sync_archive` downloads Bob's engine inputs from S3. The public
paths in `archive-artifacts.json` are shared with Gradle's artifact selection;
update that list when adding an archived input. Private-platform archive folders
retain their existing download filters for private copy hooks.

Tool packaging is defined by two additional manifests:

- `tools.json` maps full Bob's JAR entry paths to installed files relative to
  `$DYNAMO_HOME`. It includes external tools and their supporting libraries/data.
- `bob-light-tools.json` lists Ant-style JAR path patterns for Bob Light's tools,
  compiler libraries, and LuaJIT modules. The same allowlist selects private-platform
  additions. Android bundling tools such as `aapt2` are included only in full Bob.

Compiler libraries still resolve from local engine builds or `archive-artifacts.json`;
`lib/luajit-share.zip` is generated from the installed LuaJIT modules. Both JAR tasks
track the manifests as inputs, so changing a list updates the packaged contents.

Both JARs copy dependency entries directly when their compression method matches
the output, preserving the compressed bytes. Loose files are compressed in
parallel. `-Pkeep-bob-uncompressed` still stores every entry without compression;
compressed dependency entries are inflated once while writing the output.

## Content pipeline

The primary build tool is bob. Bob is used for the editor but also for engine-tests. In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar, is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.


### Byte order/endian

By convention all graphics resources are explicitly in little-endian and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar. As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is the correct order to use.


### Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.
