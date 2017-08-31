SDK
======

SDK packaging and testing support module


Build notes
----------------

The sdk module does not in itself produce a library, but acts as a module generating and testing dmsdk integrity (Defold SDK files)
This module should be built last of all librarys (see build.py, build_engine_libs).
It also generates the sdk.h file located in the root of the dmsdk includes, which contains all of the dmsdk include files.


Tests
----------------
The test is performed by building and running a simple test using the dmsdk sdk.h, so to validate that all dmsdk includes are valid
(either standard c library includes or dmsdk include files).
