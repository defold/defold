# Emscripten

## Updating emscripten version

Make sure to read the release notes of each Emscripten version you upgrade to:

https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md

### Create SDK Packages (DEPRECATED?)

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

### Rebuild 3rd party engine packages

Note: This information is based on the latest update (3.1.55 -> 3.1.65)

* update the `EMSCRIPTEN_VERSION_STR` in `scripts/build.py`
* package_emscripten:
  - update the `VERSION` to the new version
  - run the script `./script/package/package_emscripten.sh` on both OSX (x86_64 and arm64) and linux (x86_64) (tested on ubuntu 22.x)
  - copy the artifact(s) from the `local_sdk` folder to the s3-bucket `defold-packages`
* run `./scripts/build.py install_ems` to get the latest sdk for your host platform
* build protobuf for js-web, wasm-web and wasm_pthread-web (ubuntu/linux) and copy into the `defold/packages` folder
* build bullet3d for js-web, wasm-web and wasm_pthread-web (ubuntu/linux)
  - `./scripts/build.py build_external --platform=js-web`
  - `./scripts/build.py build_external --platform=wasm-web`
  - `./scripts/build.py build_external --platform=wasm_pthread-web`
  - these are automatically copied to packages
* building:
  - refresh the shell (`exit` + subsequent `scripts/build.py shell`)
  - make sure `$EMSCRIPTEN` points to the updated version
    - occasionally there has been issues with `fastcomp` vs `upstream` in the path so make sure the folder from the env variable exists

## Installation

### Using a locally installed Emscripten

As all other platforms, our builds can pick up a locally installed Emscripten by setting `EMSDK`:
```bash
> export EMSDK=/path/to/emsdk-4.x
> ./scripts/build.py shell
```

### Using a remote packaged Emscripten

For our internal team (and CI), we rely on a CDN for packages.
```bash
> export DM_PACKAGES_URL=<url>
> ./scripts/build.py install_sdk --platform=wasm-web
```

## Running tests

By installing `node.js`, you can run tests.

Installing on macOS:
```bash
> brew install node
```

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

Emscripten have several useful features for debugging, and it's really good to read their article about debugging in full (https://kripken.github.io/emscripten-site/docs/porting/Debugging.html). For general debugging it's good to read up on JavaScript maps which will be generated by emscripten if you compile with `-gsource-map`. JavaScript maps will allow the browser to translate the minified JavaScript into C/C++ file and line information so you can actually place breakpoints and watch variables from the real source code.

To debug memory and alignment issues the following parameters should be added both to `CCFLAGS`/`CXXFLAGS` and `LINKFLAGS` in `waf_dynamo.py` for the web target. It is important to note that this will decrease runtime performance, and significantly increase compile time, therefore it should only be used when debugging these kinds of issues.

- `-gsource-map` should be **added** to build with additional debug symbols.
- `-s ASSERTIONS=1` should be **added** explicitly since they are otherwise turned off by optimizations.
- `-s SAFE_HEAP=1` should be **added** to enable additional memory access checks.
- `-s STACK_OVERFLOW_CHECK=2` should be **added** to enable additional stack checks.
- `-s AGGRESSIVE_VARIABLE_ELIMINATION=1` should be **removed**, otherwise errors might be ignored.
- `-s DISABLE_EXCEPTION_CATCHING=1` should be **removed**, otherwise errors might be ignored.
