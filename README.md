# Defold

Repository for the Defold engine, editor and command line tools.


## Folder Structure

**ci** - Continuous integration related files

**com.dynamo.cr** - Bob

**engine** - Engine

**editor** - Editor

**packages** - External packages

**scripts** - Build and utility scripts

**share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.


## Setup and Build

### Setup Engine

Follow the [setup guide](README_SETUP.md) to install all of the tools needed to build the Defold engine.

### Build Engine

Follow the [build instructions](README_BUILD.md) to build the engine and command line tools.

### Setup, Build and Run Editor

Follow the [instructions](editor/README.md) in the editor folder.


## Licenses

See [licenses.md](engine/engine/content/builtins/docs/licenses.md) from `builtins.zip`


## Platform Specifics

* [iOS](README_IOS.md)
* [Android](README_ANDROID.md)
* [HTML5/Emscripten](README_EMSCRIPTEN.md)


## Content pipeline

The primary build tool is bob. Bob is used for the editor but also for engine-tests. In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar, is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.


## Byte order/endian

By convention all graphics resources are explicitly in little-endian and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar. As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is the correct order to use.


## Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.


## Asset loading

Assets can be loaded from file-system, from an archive or over http.

See *dmResource::LoadResource* for low-level loading of assets, *dmResource* for general resource loading and *engine.cpp* for initialization. A current limitation is that we don't have a specific protocol for *resource:* For file-system, archive and http url schemes *file:*, *arc:* and *http:* are used respectively. See dmConfigFile for the limitation about the absence of a resource-scheme.

### Http Cache

Assets loaded with dmResource are cached locally. A non-standard batch-oriented cache validation mechanism used if available in order to speed up the cache-validation process. See dlib, *dmHttpCache* and *ConsistencyPolicy*, for more information.


## Engine Extensions

Script extensions can be created using a simple exensions mechanism. To add a new extension to the engine the only required step is to link with the extension library and set "exported_symbols" in the wscript, see note below.

*NOTE:* In order to avoid a dead-stripping bug with static libraries on macOS/iOS a constructor symbol must be explicitly exported with "exported_symbols" in the wscript-target. See extension-test.


## Porting to another compiler

You will likely need to recompile external libraries. Check the `share/ext` folder for building the external packages for each platform (Source code for some old packages is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).)


## Code Style

Follow current code style and use 4 spaces for tabs. Never commit code with trailing white-spaces.

API documentation is generated from source comments. See [README_DOCS.md](README_DOCS.md) for help on style and conventions.


## Tagging

New tag

    # git tag -a MAJOR.MINOR [SHA1]
    SHA1 is optional

Push tags

    # git push origin --tags
