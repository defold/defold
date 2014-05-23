Script
======

Lua script support libraries


## Emscripten Notes

Due to the event-driven execution of javascript (node.js in this case) the http-tests
manually dispatch the event-loop. In order to dispatch the event-loop manually
the package uvrun is required to be installed. (https://github.com/creationix/uvrun).
See Dispatch() in test_script_http.cpp

This is considered a hack and we should potentially move the functionality to dlib or glfw.
In order to run dmengine_headless we must probably do something similar to this for the update-loop.
