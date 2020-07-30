# Engine

## Overview

The engine source code can be found in the `/engine` folder. The source code is divided into a number of different modules/folders:

### Engine libraries

To see the actual, ordered, list of engine libraries, look for `ENGINE_LIBS` in [./scripts/build.py](./scripts/build.py), and also add `dlib`, and `texc` to that list.


<details>
	<summary>List in alphabetical order:</summary>

* **crash** - Capture crashes and make them available to the `crash.*` script namespace.
* **ddf** - Defold Data Format
* **dlib** - Defold Library (http, socket, dns, sys, buffer, hash, json and much more)
* **docs** -
* **engine** - Engine create, destroy and update lifecycle
* **extension** - Extension lifecycle
* **facebook** - Facebook stub implementation. Actual implementation moved to [extension-facebook](https://github.com/defold/extension-facebook)
* **gameobject** - Game object and component lifecycle
* **gamesys** - Component and resource types are registered here
* **glfw** - Somewhat modified version of GLFW, used to set up a window/graphics context and read from input devices. Defold uses an old GLFW 2.7 version with some features backported from GLFW 3.0. We have also added support for iOS and Android.
* **graphics** - OpenGL, Vulkan (and null) implementations of graphics backend
* **gui** - The GUI component
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

</details>

## Code

One of our goals is to keep the engine as small and performant as possible.
By this we mean both development time, and actual runtime for the released game:
compiled code size, source code size, compile time, runtime.

By keeping this in mind whenever we add new features, we can keep this goal with minimal effort.

### When to add code?

The fastest code is the code that isn't run, or isn't even added!

If we don't need it, don't add it.

Think of _all_ the end users. Will the fix/feature have a clear benefit to most of our users?
If not, can the feature be added as a separate native extension instead?

#### Fixes

For the simple bug fixes, it's usually enough to add a pull request, with little to no design phase beforehand.

Comment the pull request well, and the issues it fixes.

#### Features

Before adding code to the engine, we must have a design first.

Catching problems in a design review is a huge time saver, and it also improves everyones understanding of the code and problem area.

##### Design format

For bigger changes, we need to see a design that outlines the problem, and some possible solutions.
Each solution should have its pros and cons listed, so that reviewers can reason about them.

* Keep it short
	* The document doesn't need to be fancy, or long. Aim for 1-2 pages, as long as it is easy to read and understand for the reviewers.

* Keep it on point
	* The design should only deal with the actual problem. It is easy to think in too generic terms.
The design review will also help highlight such issues.

Although we recommend starting with a design review, to get a first go-ahead, it is sometimes required to do some test
code to see if the idea pans out or not. Be aware that the community might have already touched this idea beforehand
and discarded it for one reason or another. So try to ask first, before spending too much time implementating the feature.

*Note: we still don't have a good shared place to store the design documents, where users can comment on it.
Our current best recommendation is to share a google drive document. If you have a good alternative, let us know!*

#### Backwards compatibility

Even for the least complex fixes, there might be nuances that aren't obvious at first glance.
Be aware that there are many projects out there, and we expect them to build with new versions of Defold.

Sometimes we can argue a breaking change is a bug fix, and we go ahead with the implementation.
In other cases, we need to respect the backwards compatibility.

If uncertain, ask the community for design input.

#### Incremental changes

If your feature is very large, it makes sense to break it down into smaller tasks.
This also gives other developers time to think more properly about your design of a bigger feature.

### Code Standards

#### C-like C++

Try to stay as close to C style programming as possible.
We _do_ however use a few features of C++:

* Namespaces
* RAII (in a few places, e.g. profile scopes)
* Templates (in some of our containers, and a few functions)
* Constructors of `struct` (as few as possible)

*We don't use classes, although we do on occasion use structs with constructors (which is more or less a class, with slightly less code).*

Keep these to a minimum.
In short, if we don't need it, we don't use it.

#### C++ version

Although we don't actively specify a C++ version, we aim for a very simple feature set that should work across compilers.
Essentially, the engine code use no C++ version higher than C++98.

Do not add "modern C++" features such as `auto` or lambdas.

#### No C++ Exceptions

We don’t make use of any exceptions in the engine. It isn’t generally used in game engines, since the data is (mostly) known beforehand, during development. Removing the support for C++ exceptions decreases executable size and improves the runtime performance.

#### No STL

While it might be tempting to use `std` functions/types/classes, we don't do it for several reasons.
Performance is one (code size, compilation time, debugging time), and ABI issues is another important issue.

You can read a bit more about the advice we give to extension developers here:
https://defold.com/manuals/extensions-best-practices/#standard-template-libraries---stl

We do make a (very) few exceptions, most notably for `std::sort()` and `std::upper_bound()`.
Although eventually, these may be replaced too.

#### Memory allocations

Most often we try to allocate memory upfront, when loading resources (e.g. loading a `.collection` file).
We try to avoid runtime allocations as much as possible.

We often use `free`, `malloc` and `realloc` to emphasize that the structs doesn't need a constructor.

#### Avoid defensive programming

Avoid adding defensive code such as `if (ptr != 0) *ptr = ...` as much as possible.
Checks for validity should be done as early as possible. Best option would be in the editor, then the build pipeline, the resource loading in the engine, then the Lua interface. After that, the resource should either be ok, or the owner should handle it gracefully.

Another example is setting memory to zero, just before explicitly setting the members one by one.
In such a case, the memset isn't needed.

Avoiding patterns like this keeps code size to a minimum.

#### Optimizations

When people mention optimizations, they can mean different things, and often they refer to a specific stage at the end of a project.
We want performance to be there from day one, making sure each project can be developed as quickly as possible.

We always try to think of the use cases and the expected performance in the design phase of a feature.
We are not shy to implement features ourselves, in order to reach those goals.

Also, when doing optimizations do measure them, and attach the info to the pull request, so the reviewers can make a proper assesment of the fix.
E.g. compile time, runtime, memory etc.


#### Third party libraries

Sometimes we need to use a third party library.
It is fine, if the cost of using it (code size, complexity, performance) is properly assessed.
Also make sure to list alternatives (such as implementing the feature yourself) in the design phase.

#### Debug vs Release

We don't really distinguish between the traditional Debug or Release concept.
We compile (and ship) all our libraries with the `-O2` flag.

Instead, we compartmentalize functionality into separate libraries, and during link time, either add `featureX_impl.a` or a `featureX_null.a` to the command line.

If you _do_ need to build local libraries for better denug info, use the `--opt-level=0` flag on the command line.
You can also use the `./scripts/submodule.sh` to rebuild a single library with `O0` and then relink the engine (usually the fastest option).

#### Defines

We do not use any release/debug defines, but rather compiler specific defines to control certain behavior when required.
Most often, this occurs in the lower layers of the engine, such as `dlib`.

#### Platform differences

For small differences, we use compiler defines directly in a function.
For larger differences, we put them into separate files. E.g. `file_win32.cpp` vs `file_posix.cpp`.

## Content pipeline

The primary build tool is bob. Bob is used for the editor but also for engine-tests. In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar, is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.


### Byte order/endian

By convention all graphics resources are explicitly in little-endian and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar. As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is the correct order to use.


### Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.


### Asset loading

Assets can be loaded from file-system, from an archive or over http.

See *dmResource::LoadResource* for low-level loading of assets, *dmResource* for general resource loading and *engine.cpp* for initialization. A current limitation is that we don't have a specific protocol for *resource:* For file-system, archive and http url schemes *file:*, *arc:* and *http:* are used respectively. See dmConfigFile for the limitation about the absence of a resource-scheme.

#### Http Cache

Assets loaded with dmResource are cached locally. A non-standard batch-oriented cache validation mechanism used if available in order to speed up the cache-validation process. See dlib, *dmHttpCache* and *ConsistencyPolicy*, for more information.


### Engine Extensions

Script extensions can be created using a simple extensions mechanism. To add a new extension to the engine the only required step is to link with the extension library and set "exported_symbols" in the wscript, see note below.

*NOTE:* In order to avoid a dead-stripping bug with static libraries on macOS/iOS a constructor symbol must be explicitly exported with "exported_symbols" in the wscript-target. See extension-test.


### Porting to another compiler

You will likely need to recompile external libraries. Check the `share/ext` folder for building the external packages for each platform (Source code for some old packages is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).)


### Code Style and API documentation

Follow current code style as defined in `.clang-format`

API documentation is generated from source comments. See [README_DOCS.md](README_DOCS.md) for help on style and conventions.

*Note: this is a new file trying to retro fit into current style. If it alters the style of our current files too much, it probably needs adjusting.*
