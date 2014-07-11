Script
======

C functions exposed to Lua
--------------------------

* Check the type of all input arguments
* Assert that the lua stack (lua_gettop) has changed as expected before each exit point
* If the function takes a callback function as an input parameter, the callback function must be run on the lua main thread.
  This can be retrieved from any lua_State by calling dmScript::GetMainThread.
  The reason for this is to handle coroutines making the original call, yielding and then being resumed from the callback function.
