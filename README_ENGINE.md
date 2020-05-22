# Engine

## Overview

The engine source code can be found in the `/engine` folder. The source code is divided into a number of different modules/folders:

* **crash** - Capture crashes and make them available to the `crash.*` script namespace.
* **ddf** - Defold Data Format
* **dlib** - Defold Library (http, socket, dns, sys, buffer, hash, json and much more)
* **docs** -
* **engine** - Engine create, destroy and update lifecycle
* **extension** - Extension lifecycle
* **facebook** - Facebook stub implementation. Actual implementation moved to [extension-facebook](https://github.com/defold/extension-facebook)
* **gameobject** - Game object and component lifecycle
* **gamesys** - Component and resource types are registered here
* **glfw** - Somewhat modified version of GLWF, used to set up a window/graphics context and read from input devices. Defold uses an old GLFW 2.7 version with some features backported from GLFW 3.0.
* **graphics** - OpenGL, Vulkan (and null) implementation of graphics backend
* **gui** - Component
* **hid** - Human interface device systems (GLFW and null)
* **iac** - Inter-App Communication stub implementation. Actual implementation moved to [extension-iac](https://github.com/defold/extension-iac)
* **iap** - In-App Purchases stub implementation. Actual implementation moved to [extension-iap](https://github.com/defold/extension-iap)
* **input** - Input bindings and propagation of input actions to listeners. Reads input data from **hid**.
* **liveupdate** - Resource update system
* **lua** - Lua 5.1.4
* **particle** - Component
* **physics** - 2D and 3D physics
* **profiler** - Engine profiler
* **push** - Push notification stub implementation. Actual implementation moved to [extension-push](https://github.com/defold/extension-push)
* **record** - Screen recorder
* **render** - Rendering pipeline
* **resource** - Resource system (create, load, destroy, ref count etc)
* **rig** - Spine and 3D model animation rig
* **script** - Lua bindings
* **sdk** - Defold SDK (for use by native extensions)
* **sound** - Sound systems
* **texc** - Texture compression
* **tools** - Tools such as the gamepad configurator
* **webview** - WebView stub implementation. Actual implementation moved to [extension-webview](https://github.com/defold/extension-webview)


## C++ version
We use no C++ version higher than C++98.


## No C++ Exceptions
We don’t make use of any exceptions in the engine. It isn’t generally used in game engines, since the data is (mostly) known beforehand, during development. Removing the support for C++ exceptions decreases executable size and improves the runtime performance.


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

Script extensions can be created using a simple extensions mechanism. To add a new extension to the engine the only required step is to link with the extension library and set "exported_symbols" in the wscript, see note below.

*NOTE:* In order to avoid a dead-stripping bug with static libraries on macOS/iOS a constructor symbol must be explicitly exported with "exported_symbols" in the wscript-target. See extension-test.


## Porting to another compiler

You will likely need to recompile external libraries. Check the `share/ext` folder for building the external packages for each platform (Source code for some old packages is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).)


## Code Style and API documentation

Follow current code style as defined in `.clang-format`

API documentation is generated from source comments. See [README_DOCS.md](README_DOCS.md) for help on style and conventions.
