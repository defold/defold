![CI - Main](https://github.com/defold/defold/workflows/CI%20-%20Main/badge.svg)
![CI - Editor Only](https://github.com/defold/defold/workflows/CI%20-%20Editor%20Only/badge.svg)
![CI - Engine nightly](https://github.com/defold/defold/workflows/CI%20-%20Engine%20nightly/badge.svg)

# Defold

Repository for the Defold engine, editor and command line tools.

## Supported by
[![](https://defold.com/images/logo/others/king-color.png)](https://www.king.com)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/heroiclabs-blue.png)](https://www.heroiclabs.com)

## Folder Structure

* **build_tools** - Build configuration and build tools used by build scripts
* **ci** - Continuous integration files for GitHub CI ([more info](README_CI.md))
* **com.dynamo.cr** - Bob
* **engine** - Engine
* **editor** - Editor
* **packages** - External packages
* **scripts** - Build and utility scripts
* **share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.

## Setup and Build

### Setup Engine

Follow the [setup guide](README_SETUP.md) to install all of the tools needed to build the Defold engine.

### Build Engine

Follow the [build instructions](README_BUILD.md) to build the engine and command line tools.

### Setup, Build and Run Editor

Follow the [instructions](editor/README.md) in the editor folder.

## Engine Overview

An overview of the engine architecture and additional engine information can be [viewed here](README_ENGINE.md).

### Platform Specifics

* [iOS](README_IOS.md)
* [Android](README_ANDROID.md)
* [HTML5/Emscripten](README_EMSCRIPTEN.md)

## Releasing a new version

The release process is documented [here](RELEASE.md).


## Third party licenses

See [licenses.md](engine/engine/content/builtins/docs/licenses.md) from `builtins.zip` for licenses of libraries and source code used in this project.
