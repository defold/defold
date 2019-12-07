# Build Engine

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

## Windows

Start msys by running `C:\MinGW\msys\1.0\msys.bat`. You should now have a bash prompt. Verify that:

- `which git` points to the git from the windows installation instructions above
- `which javac` points to the `javac.exe` in the JDK directory
- `which python` points to `/c/Python27/python.exe`

Note that your C: drive is mounted to /c under MinGW/MSYS.

Next, `cd` your way to the directory you cloned Defold into.

    $ cd /c/Users/erik.angelin/Documents/src/defold

Setup the build environment. The `--platform` argument is needed for instance to find the version of `go` relevant for the target platform.

	$ ./scripts/build.py shell --platform=x86_64-win32

This will start a new shell and land you in your msys home (for instance `/usr/home/Erik.Angelin`) so `cd` back to Defold.

    $ cd /c/Users/erik.angelin/Documents/src/defold

Install external packages. This step is required whenever you switch target platform, as different packages are installed.

	$ ./scripts/build.py install_ext --platform=x86_64-win32

Now, you should be able to build the engine.

	$ ./scripts/build.py build_engine --platform=x86_64-win32 --skip-tests -- --skip-build-tests

When the initial build is complete the workflow is to use waf directly.

    $ cd engine/dlib
    $ waf

## OS X/Linux

Setup the build environment. The `--platform` argument is needed for instance to find the version of `go` relevant for the target platform..

    $ ./scripts/build.py shell --platform=...

Install external packages. This step is required whenever you switch target platform, as different packages are installed.

    $ ./scripts/build.py install_ext --platform=...

Build engine for target platform.

    $ ./scripts/build.py build_engine --skip-tests --platform=...

Build at least once with 64 bit support (to support the particle editor, i.e. allowing opening collections).

    $ ./scripts/build.py build_engine --skip-tests --platform=x86_64-darwin

When the initial build is complete the workflow is to use waf directly:

    $ cd engine/dlib
    $ waf

# Unit tests

Unit tests are run automatically when invoking waf if not --skip-tests is specified. A typically workflow when working on a single test is to run:

    $ waf --skip-tests && ./build/default/.../test_xxx

With the flag `--gtest_filter=` it's possible to run a single test in the suite, see [Running a Subset of the Tests](https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_a_Subset_of_the_Tests)
