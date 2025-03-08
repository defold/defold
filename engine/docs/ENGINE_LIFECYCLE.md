# Engine life cycle

Here we describe the flow of the engine, from engine start to engine finish.


## Engine main

The engine entry point is in [engine_main.cpp](../engine/src/engine_main.cpp).
It has a function called `engine_main()` which gets called from actual `main()` function.
The main function is usually located in [engine/src/common/main.cpp](engine/src/common/main.cpp), but it may vary, especially on consoles.

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

The create function creates a new engine instance ([dmEngine::New()](../engine/src/engine.cpp)), and calls [dmEngine::Init()](../engine/src/engine.cpp) on it. Similarly, the [dmEngine::Destroy()](../engine/src/engine.cpp) is called during destruction.

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

Note that the [dmScript::Initialize()]() is in fact also calling `Initialize()` on our `Native Extensions`.
This is because they were initially implemented as extensions to the the script system (for adding Lua modules).

The destroy functions are called in reverse order.

### The Engine Update

The engine update is calling [dmEngineUpdate()](../engine/src/engine.cpp), which is in turn calling [dmEngine::Step()](../engine/src/engine.cpp).

This function is what define the "game frame" in the engine.
Here is a short description of the loop.
(For full info, we recommend you read the [the source](../engine/src/engine.cpp))

```python
function StepFrame:
    ResourceUpdate() # Checking resource "Reload" messages

    HIDUpdate()

    JobsUpdate()

    ScriptExtensionsUpdate() # E.g native extensions

    SoundUpdate()

    InputUpdate()

    GameObjectUpdate() # Updates collections, game objects and components
                       # The loading of collections etc is also handled here

    if not IsWindowMinimized:
        continue

        GraphicsBegin()

        RenderScriptUpdate() # schedules all the graphics commands

        GraphicsDraw() # Renders all graphics commands

        GraphicsEnd()


    GameObjectPostUpdate()

    DispatchMessages() # Important to note it's not the only place where do this!
```


