# Engine life cycle

Here we describe the flow of the engine, from engine start to engine finish.


## Engine main

The engine entry point is in [engine_main.cpp](../engine/src/engine_main.cpp).
It has a function called `engine_main()` which gets called from actual `main()` function.
The main function is usually located in [engine/src/common/main.cpp](../engine/src/common/main.cpp), but it may vary, especially on consoles.

The `engine_main()` is responsible for starting the Engine Loop, and also calling `dmExportedSymbols()` which is making sure that each Native Extension gets registered.


## Engine Loop

The engine loop is found in [engine_loop.cpp](../engine/src/engine_loop.cpp).
It is responsible for creating, destroying and updating the engine instance.

```python
AppCreate()

running = True
engine = null
while running
    if engine == null
        engine = CreateEngine()

    result = engine.Update()
    if result != OK
        engine.Destroy()
        engine = null
        if result != REBOOT
            running = False

AppDestroy()
```

### Reboot

The reboot sequence is connected to the lua function [sys.reboot()](https://defold.com/ref/stable/sys/#sys.reboot), which is mainly used if you have updated content via live update and need to restart the engine (it's not always the case you need to restart)

### App Create / Destroy

The app create/destroy are calling [dmEngineInitialize()](../engine/src/engine.cpp)/[dmEngineFinalize()](../engine/src/engine.cpp) respectively.
Here we run things that are only supposed to be initialized/destroyed once per session, e.g. Logs, Sockets, Debug service etc.

The initialize/finalize functions are also calling any platform specific functions that are needed to setup the app.

### Engine Create / Destroy

The engine create/destroy are calling [dmEngineCreate()](../engine/src/engine.cpp)/[dmEngineDestroy()](../engine/src/engine.cpp) respectively.

The create function creates a new engine instance ([dmEngine::New()](../engine/src/engine.cpp)), and calls [dmEngine::Init()](../engine/src/engine.cpp) on it. Similarly, [dmEngine::Delete()](../engine/src/engine.cpp) is called during destruction.

The [dmEngine::Init()](../engine/src/engine.cpp) is quite long and won't be described in detail here, see the source code for that instead.

The init function is responsible for:

* Loading the `game.projectc`
* Native Extensions `AppInitialize()`
* Setting up each internal systems
  * HID + Input
  * Window
  * Graphics
  * Resource System
  * Gameobject registry
  * Registering Component types
  * Registering Resource types
  * Physics
  * Sound
  * Scripting
    * Lua context
    * Native Extensions `Initialize()`
  * Debug web server
  * ...

The engine initializes the Lua context with `dmScript::Initialize()`, then passes it to `dmExtension::Initialize()` so native extensions can register their Lua modules. Both calls are in [engine.cpp](../engine/src/engine.cpp).

The destroy functions are called in reverse order.

### The Engine Update

The engine update is calling [dmEngineUpdate()](../engine/src/engine.cpp), which is in turn calling [dmEngine::Step()](../engine/src/engine.cpp).

A normal frame polls input, processes jobs and resource reloads, updates scripts and native extensions, dispatches input to game objects, then updates the collection and sound. The engine submits rendering when rendering is enabled and the window is visible. Collection post-update and system message dispatch follow. The `engine.run_while_iconified` setting controls whether simulation continues while the window is minimized.

See `StepFrame` in [engine.cpp](../engine/src/engine.cpp) for the full order, including service updates and the frame flip.
