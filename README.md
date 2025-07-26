![CI - Main](https://github.com/defold/defold/workflows/CI%20-%20Main/badge.svg)
![CI - Editor Only](https://github.com/defold/defold/workflows/CI%20-%20Editor%20Only/badge.svg)
![CI - Engine nightly](https://github.com/defold/defold/workflows/CI%20-%20Engine%20nightly/badge.svg)

[![Join the chat at https://discord.gg/cHBde7J](https://img.shields.io/discord/250018174974689280?color=%237289DA&label=defold&logo=discord&logoColor=white)](https://discord.gg/cHBde7J)

# Defold

Repository for the Defold engine, editor and command line tools.

## Supported by
[![](https://defold.com/images/logo/others/melsoft-black.png)](https://melsoft-games.com/)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/rive-black.png)](https://www.rive.app)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/poki-black.png)](https://www.poki.com)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/op-games-color.png)](https://www.opgames.org)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/heroiclabs-blue.png)](https://www.heroiclabs.com)
![](https://defold.com/images/spacer32.png)
[![](https://defold.com/images/logo/others/king-color.png)](https://king.com/)

## Folder Structure

* **build_tools** - Build configuration and build tools used by build scripts
* **ci** - Continuous integration files for GitHub CI ([more info](README_CI.md))
* **com.dynamo.cr** - Bob
* **engine** - Engine
* **editor** - Editor
* **external** - External libraries that can be rebuilt using our build system
* **packages** - Prebuilt external packages
* **scripts** - Build and utility scripts
* **share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.
* **share/ext** - External libraries that are built using custom build steps

## Setup and Build

### Setup Engine

Follow the [setup guide](README_SETUP.md) to install all of the tools needed to build the Defold engine.

### Build Engine

Follow the [build instructions](README_BUILD.md) to build the engine and command line tools.

### Setup, Build and Run Editor

Follow the [instructions](editor/README.md) in the editor folder.

## Engine Overview

An overview of the engine architecture and additional engine information can be [viewed here](./engine/docs/README.md).

### Platform Specifics

* [iOS](README_IOS.md)
* [Android](README_ANDROID.md)
* [HTML5/Emscripten](README_EMSCRIPTEN.md)

## Releasing a new version

The release process is documented [here](RELEASE.md).

## Complying with licenses

A full list of third party software licenses along with information on how to give attribution and include the licenses in your game can be found in the [COMPLYING WITH LICENSES](/COMPLYING_WITH_LICENSES.md) document in the Defold repository on GitHub.
