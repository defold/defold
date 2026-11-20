# The Defold engine code structure

## Browsing the engine code

To make it easier to find your way around the code, it's good to know about some of our naming conventions.

### Naming conventions

* `res_*.cpp` - Resource types. These are responsible for loading/unloading a certain (compiled) asset type. E.g. [res_sprite.cpp](../gamesys/src/gamesys/resources/res_sprite.cpp)

* `comp_*.cpp` - Component types. Reponsible for creating a component world and instances. Matched with a resource type using the same filename extension. E.g. [comp_sprite.cpp](../gamesys/src/gamesys/components/comp_sprite.cpp)

* `script_*.cpp` - Script modules. These add new Lua modules to the engine. E.g. [script_sprite.cpp](../gamesys/src/gamesys/scripts/script_sprite.cpp)
  * Exceptions to the rule: `gameobject_script.cpp`, `gui_script.cpp` and `render_script.cpp`

* `*_ddf.proto` - File formats. These are the file formats used for source and compiled content. [sprite_ddf.proto](../gamesys/proto/gamesys/sprite_ddf.proto)


### Libraries

The top-level [CMakeLists.txt](../../CMakeLists.txt) lists the engine modules in `DEFOLD_ENGINE_LIBS`. Each module declares its own targets and dependencies.

The order of these libraries is important as they determine the possible ways a library may use another library.
E.g. `render` may use `graphics`, but the reverse order is not allowed.

* testmain - Helper library for unit tests and platform specific functionality.
* dlib - Our main util library with containers, sockets, I/O, compression etc
* [datastore](../datastore/README.md) - Typed data storage and job scheduling.
* jni - Support for calling native engine tools from Java.
* texc - Texture compiler. Builds `texturecompiler.jar` and a native shared library for Bob.
* [modelc](../modelc/README.md) - Model importer. Builds `modelimporter.jar` and a native shared library for Bob.
* shaderc - Shader compiler and Java bindings.
* ddf - Our runtime data format. It's a slimmed down version of Protobuf.
* platform - Handles some platform specific concepts, like a Window.
* graphics - Graphics layer with e.g. vertex buffers and shader programs. Backends for OpenGL, Vulkan etc.
* [font](../font/README.md) - Font processing, layout and rendering data.
* particle - Simulates particles for the [particle effect component](../gamesys/src/gamesys/components/comp_particlefx.cpp).
* lua - The regular Lua 5.1 implementation.
* hid - Human Interface Device library that handles gamepads, mouse, keyboards and touch input.
* input - A higher level input system that maps HID events into input actions
* physics - A single frontend for our two physics engines Box 2D and Bullet 3D. Both Box2D and Bullet are built from sources in `external/`.
* resource - Resource system. Handles creation of resources, reference counting, and also mounting of resource archives.
* extension - API for registering native extensions (aka script extensions)
* [script](../script/README.md) - A high level scripting interface that uses Lua as the backend.
* [debugger](../debugger/README.md) - Native Debug Adapter Protocol support for Lua.
* render - High level rendering concepts like materials, constants, cameras and render command buffer.
* rig - Our model+skeleton animation system.
* gameobject - Collections, game objects and component base functionality
* gui - Our gui system. Used by `comp_gui.cpp`
* [sound](../sound/README.md) - Our software sound mixing library
* liveupdate - High level functionality for downloadable content
* crash - Callstack callbacks and writing functionality
* gamesys - The main bulk of all resource types, component types and script modules.
* tools - Utilities like the `gdc` and the editor `launcher`
* [record](../record/README.md) - Captures H.264 video in MP4 files on desktop debug engines.
* profiler - High level profiler api (using functionality from dlib's profile.h)
* engine - The defold engine libraries and executable
* [sdk](../sdk/README.md) - Extra tests for our `dmSDK` headers.


### Headers

When building the engine, some headers are copied from the local library folder, to the `DYNAMO_HOME/include` sub folders.

Public headers from the modules’ `dmsdk` directories are installed under `$DYNAMO_HOME/sdk/include/dmsdk/` and shipped in the native extension SDK.

If you edit any of those header files, be aware of any breaking changes to the api!


### Bob.jar

Our command line content build system is called `Bob.jar`.
It is [described here](../../com.dynamo.cr/README.md)

### External libraries and packages

Run `./scripts/build.py install_ext` from the repository root. It unpacks prebuilt dependencies from [packages](../../packages/) and builds source dependencies from [external](../../external/README.md) into `$DYNAMO_HOME/ext`. These include Box2D and Bullet for the runtime, and Protobuf for desktop tools.

[share/ext](../../share/ext/) contains custom packaging scripts for dependencies such as GLFW 3 and Tremolo. The mobile and web window backends build as part of [engine/platform](../platform/src/native/README.md).
