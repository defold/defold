Script
======

Lua script support libraries


Emscripten notes
----------------

Due to the event-driven execution of javascript (node.js in this case) the http-tests
manually dispatch the event-loop. In order to dispatch the event-loop manually
the package uvrun is required to be installed. (https://github.com/creationix/uvrun).
See Dispatch() in test_script_http.cpp

This is considered a hack and we should potentially move the functionality to dlib or glfw.
In order to run dmengine_headless we must probably do something similar to this for the update-loop.


C functions exposed to Lua
--------------------------

* Check the type of all input arguments
* Assert that the lua stack (lua_gettop) has changed as expected before each exit point
* If the function takes a callback function as an input parameter, the callback function must be run on the lua main thread.
  This can be retrieved from any lua_State by calling dmScript::GetMainThread.
  The reason for this is to handle coroutines making the original call, yielding and then being resumed from the callback function.

