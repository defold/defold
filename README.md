Defold
======

Repository for engine, editor and server.


Tagging
-------

New tag

    # git tag -a MAJOR.MINOR [SHA1]
    SHA1 is optional

Push tags

    # git push origin --tags


Folder Structure
----------------

**ci** - Continious integration related files

**com.dynamo.cr** - _Content repository_. Editor and server

**engine** - Engine

**packages** - External packages

**scripts** - Build and utility scripts

**share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.

iOS Debugging
-------------

* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Project>New Scheme...
* Select executable
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason

