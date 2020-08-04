# Emscripten

**TODO**

* Run all tests
* In particular (LuaTableTest, Table01) and setjmp
* Profiler (disable http-server)
* Non-release (disable engine-service)
* Verify that exceptions are disabled
* Alignments. Alignment to natural boundary is required for emscripten. uint16_t to 2, uint32_t to 4, etc
  However, unaligned loads/stores of floats seems to be valid though.
* Create a node.js package with uvrun for all platforms (osx, linux and windows)

## Create SDK Packages

* Download [emsdk_portable](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html)
* Compile on 32-bit Linux
* Run `emsdk update` and `emsdk install`
* On Linux first remove the following directories
  - `emsdk_portable/clang/fastcomp/src`
  - Everything **apart** from `emsdk_portable/clang/fastcomp/build_master_32/bin`
  - Strip debug information from files in `emsdk_portable/clang/fastcomp/build_master_32/bin`
* Create a tarball of the package
* Upload packages to s3-bucket `defold-packages`

In order to run on 64-bit Ubuntu install the following packages `lib32z1 lib32ncurses5 lib32bz2-1.0 lib32stdc++6`

## Installation

To install the emscripten tools, invoke 'build.py install_ems'.

Emscripten creates a configuration file in your home directory (~/.emscripten).Should you wish to change branches to one
in which a different version of these tools is used then call 'build.py activate_ems' after doing so. This will cause the .emscripten file to be updated.

Emscripten also relies upon python2 being on your path. You may find that this is not the case (which python2), it should be sufficient to create a symbolic link to
the python binary in order to solve this problem.

waf_dynamo contains changes relating to emscripten. The simplest way to collect these changes is to run 'build_ext'

    > scripts/build.py install_ext

Building for js-web requires installation of the emscripten tools. This is a slow process, so not included in install_ext, instead run install_ems:

    > scripts/build.py install_ems

As of 1.22.0, the emscripten tools emit separate *.js.mem memory initialisation files by default, rather than embedding this data directly into files.
This is more efficient than storing this data as text within the javascript files, however it does add to a new set of files to include in the build process.
Should you wish to manually update the contents of the editor's engine files (com.dynamo.cr.engine/engine/js-web) then remember to include these items in those
that you copy. Build scripts have been updated to move these items to the expected location under *DYNAMO HOME* (bin/js-web), as has the copy_dmengine.sh script.

## Running Headless Builds

In order to run headless builds of the engine, take the following steps:

* Ensure that you have installed the [xhr2 node module](https://www.npmjs.org/package/xhr2)
* Select or create a folder in which to run your test
* Copy dmengine_headless.js, dmengine_headless.js.mem, game.darc and game.projectc into your folder
* Run dmengine_headless.js with node.js

Since game.darc and game.projectc are not platform specific, you may copy these from any project bundle built with the same engine version that you wish to
test against.

When running headless builds, you may also find it useful to install [node-inspector](https://github.com/node-inspector/node-inspector). Note that it operates on
port 8080 by default, so either close your Defold tools or change this port when running such builds.

To get working keyboard support (until our own glfw is used or glfw is gone):
- In ~/local/emscripten/src/library\_glfw.js, on row after glfwLoadTextureImage2D: ..., add:
glfwShowKeyboard: function(show) {},

To use network functionality during development (or until cross origin support is added to QA servers):
- google-chrome --disable-web-security
- firefox requires a http proxy which adds the CORS headers to the web server response, and also a modification in the engine is required.

Setting up Corsproxy with defold:
To install and run the corsproxy on your network interface of choice for example 172.16.11.23:
```sh
sudo npm install -g corsproxy
corsproxy 172.16.11.23
```

Then, the engine needs a patch to change all XHR:s:
- remove the line engine/script/src/script_http_js.cpp:
```
xhr.open(UTF8ToString(method), UTF8ToString(url), true);
```
- and add
```
var str_url = UTF8ToString(url);
str_url = str_url.replace("http://", "http://172.16.11.23:9292/");
str_url = str_url.replace("https://", "http://172.16.11.23:9292/");
xhr.open(UTF8ToString(method), str_url, true);
```

For faster builds, change in scripts/build.py -O3 to -O1 in CCFLAGS, CXXFLAGS and LINKFLAGS
To profile in the browser, add -g2 to CCFLAGS, CXXFLAGS and LINKFLAGS. This will cause function names and whitespaces to remain in the js file but also increases the size of the file.

Some flags that is useful for emscripten projects would be to have:
-s ERROR_ON_UNDEFINED_SYMBOLS=1
'-fno-rtti'. Can't be used at the moment as gtest requires it, but it would be nice to have enabled

## Debugging

Emscripten have several useful features for debugging, and it's really good to read their article about debugging in full (https://kripken.github.io/emscripten-site/docs/porting/Debugging.html). For general debugging it's good to read up on JavaScript maps which will be generated by emscripten if you compile with `-g4`. JavaScript maps will allow the browser to translate the minified JavaScript into C/C++ file and line information so you can actually place breakpoints and watch variables from the real source code.

To debug memory and alignment issues the following parameters should be added both to `CCFLAGS`/`CXXFLAGS` and `LINKFLAGS` in `waf_dynamo.py` for the web target. It is important to note that this will decrease runtime performance, and significantly increase compile time, therefore it should only be used when debugging these kinds of issues.

- `-g4` should be **added** to build with additional debug symbols.
- `-s ASSERTIONS=1` should be **added** explicitly since they are otherwise turned off by optimizations.
- `-s SAFE_HEAP=1` should be **added** to enable additional memory access checks.
- `-s STACK_OVERFLOW_CHECK=2` should be **added** to enable additional stack checks.
- `-s AGGRESSIVE_VARIABLE_ELIMINATION=1` should be **removed**, otherwise errors might be ignored.
- `-s DISABLE_EXCEPTION_CATCHING=1` should be **removed**, otherwise errors might be ignored.
