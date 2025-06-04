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

This is the list of libraries in the `defold/engine/<library>` folder that are used to make up the engine.
For the up-to-date list of libraries, see the `ENGINE_LIBS` in [build.py](../../scripts/build.py)

The order of these libraries is important as they determine the possible ways a library may use another library.
E.g. `render` may use `graphics`, but the reverse order is not allowed.

* testmain - Helper library for unit tests and platform specific functionality.
* dlib - Our main util library with containers, sockets, I/O, compression etc
* texc - Our texture compiler. Creates a .jar+.dll for use with bob.jar
* [modelc](../modelc/README.md) - Our model compiler. Creates a .jar+.dll for use with bob.jar
* ddf - Our runtime data format. It's a slimmed down version of Protobuf.
* platform - Handles some platform specific concepts, like a Window.
* graphics - Graphics layer with e.g. vertex buffers and shader programs. Backends for OpenGL, Vulkan etc.
* particle - A system for simulating particles. Used by comp_particle.cpp
* lua - The regular Lua 5.1 implementation.
* hid - Human Interface Device library that handles gamepads, mouse, keyboards and touch input.
* input - A higher level input system that maps HID events into input actions
* physics - A single frontend for our two physics engines Box 2D and Bullet 3D. Includes the source of Box2D (Bullet 3D is in `defold/external`, see notes below)
* resource - Resource system. Handles creation of resources, reference counting, and also mounting of resource archives.
* extension - API for registering native extensions (aka script extensions)
* [script](../script/README.md) - A high level scripting interface that uses Lua as the backend.
* render - High level rendering concepts like materials, constants, cameras and render command buffer.
* rig - Our model+skeleton animation system.
* gameobject - Collections, game objects and component base functionality
* gui - Our gui system. Used by `comp_gui.cpp`
* [sound](../sound/README.md) - Our software sound mixing library
* liveupdate - High level functionality for downloadable content
* crash - Callstack callbacks and writing functionality
* gamesys - The main bulk of all resource types, component types and script modules.
* tools - Utilities like the `gdc` and the editor `launcher`
* record - Video (.avi) recording library. Current video format is the open vp8 codec in the ivf container.
* profiler - High level profiler api (using functionality from dlib's profile.h)
* engine - The defold engine libraries and executable
* [sdk](../sdk/README.md) - Extra tests for our `dmSDK` headers.


### Headers

When building the engine, some headers are copied from the local library folder, to the `DYNAMO_HOME/include` sub folders.

Similarly, we copy header files from any `dmsdk` sub folders from each library.
These end up in the final SDK that our users use on a daily basis.

If you edit any of those header files, be aware of any breaking changes to the api!


### Bob.jar

Our command line content build system is called `Bob.jar`.
It is [described here](../../com.dynamo.cr/README.md)

### External libraries

We keep certain versions of external code in `defold/share/ext`
Since they change so rarely, we don't want them building in our main build step.

Examples are `protobuf`, `tremolo` and `luajit`

The output is stored in the repository, under `defold/packages`

### External libraries pt 2

Since the projects under `defold/share/ext` are farily cumbersome to maintain and build, we have moved some to `defold/external`.

The benefit is that we can use the same build system for all platforms, which makes it easy to maintain such a library.

Currently includes `Bullet 3D` and `glfw`.

The output is stored in the repository, under `defold/packages`

See the [readme](../../external/README.md) for more detailed info.

### Packages

Ìn `defold/packages` we store code/tools that are rarely updated.
We unpack these packages with `./build.py install_ext` and they're installed into the `DYNAMO_HOME` folder.


