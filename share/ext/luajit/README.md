

# Notes

`luajit-32` for MacOS cannot currently be built with latest LuaJIT.
We therefore use the prebuilt version.
An option is to use a 32 bit VM of an old osx


## Win32 + Win64

Start a "Native Tools Command Prompt for VS" for x86 and x64 respectively.
Use the buld_win32.bat to download, build and package the bundle

Needs the `git` tool to apply the patch.

## PS4/PS5

Start a "Native Tools Command Prompt for VS" for x64.
Make sure you've set the necessary root SDK environment variables.
Use the build_ps4.bat/build_ps5.bat to download, build and package the bundle.

## Switch

Start a "Native Tools Command Prompt for VS" for x64.
Make sure you've set the necessary root SDK environment variables.
Use the build_nx64.bat to download, build and package the bundle.

## `armv7-android`, `arm64-android` + `x86_64-linux`

In order to build these platforms more easily it's easier to use a docker container.

    $ ./scripts/docker/build.sh
    $ ./scripts/docker/run.sh

Now you have a Ubuntu instance running.
Set the DYNAMO_HOME variable:

    $ ./scripts/build.py shell

Install linux tools.
Note that you may need to remove the previous android sdk/ndk tools from DYNAMO_HOME.

    $ ./scripts/build.py install_sdk --platform=armv7-android

Change directory to luajit and build as usual.
