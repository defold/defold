

# Notes

`luajit-32` for MacOS cannot currently be built with latest LuaJIT.
We therefore use the prebuilt version


## `armv7-darwin`

(note, the next time we upgrade, we probably aren't supporting this architecture anymore)

To build a version for `armv7-android`, we neede 32 bit executable support on the host. In order to do that, we installed a Mac OS 10.14 VM in Parallels Desktop.

IN the common.sh script I set:

DARWIN_TOOLCHAIN_ROOT=${SCRIPTDIR}/luajit/sdk/XcodeDefault.xctoolchain


## `armv7-android`

In order to build this platform more easily (see the above note on `armv7-darwin` it's easier to use a docker container.

    $ ./scripts/docker/build.sh
    $ ./scripts/docker/run.sh

Now you have a Ubuntu instance running.
Set the DYNAMO_HOME variable:

    $ ./scripts/build.py shell

Install linux tools.
Note that you may need to remove the previous android sdk/ndk tools from DYNAMO_HOME.

    $ ./scripts/build.py install_ext --platform=armv7-android

Change directory to luajit and build as usual.