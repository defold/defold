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

Content pipeline
----------------

The primary build tool is bob. Bob is used for the editor but also for engine-tests.
In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar,
is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x
is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled
, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.

Byte order/endian
-----------------

By convention all graphics resources are expliticly in little-ending and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support
only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar.
As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is
the correct order to use.


iOS Debugging
-------------

* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Project>New Scheme...
* Select executable
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason


Android testing
---------------
Copy executable (or directory) with

    # adb push <DIR_OR_DIR> /data/local/tmp

When copying directories append directory name to destination path. It's oterhwise skipped

Run exec with:

    # adb shell /data/local/tmp/....

For interactive shell run "adb shell"


Android debugging
-----------------

* Go to application bundle-dir in build/default/...,  e.g. build/default/examples/simple_gles2.android
* Install and launch application
* Run ndk-gdb from android ndk
* Debug

OpenGL and jogl
---------------

Prior to GLCanvas#setCurrent the GLDrawableFactory must be created on OSX. This might be a bug but the following code works:

        GLDrawableFactory factory = GLDrawableFactory.getFactory(GLProfile.getGL2ES1());
        this.canvas.setCurrent();
		this.context = factory.createExternalGLContext();

Typically the getFactory and createExternalGLContext are in the same statement. The exception thrown is "Error: current Context (CGL) null, no Context (NS)" and might be related to loading of shared libraries that seems to triggered when the factory is
created. Key is probably that GLCanvas.setCurrnet fails to set current context before the factory is created. The details
are unknown though.

