# Switch

## Environment Setup

* Nintendo Developer Interface
** Install a new SDK
*** Add environment
*** Create my own
*** Install to disk
*** Platform: Nintendo Switch, Toolset: Standard
*** Select latest version
*** Choose location (e.g. c:/Nintendo/) and name (e.g. SDK). This will become your %NINTENDO_SDK_ROOT%
*** Choose packages: Developer SDK + NW
*** Install

* Visual Studio 2019

* See [README_SETUP.md](README_SETUP.md) on how to install Java and Msys etc.

### Build

	$ ./scripts/build.py shell
	$ ./scripts/build.py install_ext --platform=arm64-nx64
	$ ./scripts/build.py build_engine --platform=arm64-nx64

To build without rebuilding bob:

	$ ./scripts/build.py build_engine --platform=arm64-nx64 --skip-bob-light

To build without rebuilding tests:

	$ ./scripts/build.py build_engine --platform=arm64-nx64 --skip-tests -- --skip-build-tests

To build with verbose output:

	$ ./scripts/build.py build_engine --platform=arm64-nx64 -- --verbose

To build with O0:

	$ ./scripts/build.py build_engine --platform=arm64-nx64 -- --opt-level=0

To build just one engine library (e.g. `switch`) (plus relink exe):

	$ ./scripts/submodule.sh arm64-nx64 switch

### Run

Run a bundle (without uploading):

	$ RunOnTarget.exe bundle.nspd

Install a bundle:

	
### Debug

* Start Visual Studio 2019
* When the `Open Project` wizard is open, drag the `bundle.nspd_root` file into it and it will open.
* Press F5 to Launch
* When choosing user, you can load the symbols by choosing the `build/arm64-nx64/dmengine.nss`


## Game Setup

### File System

#### Cache storage

Since the developer has to apply for cache storage from Nintendo, it might not be beneficial to ship with this feature.
However, some features are are dependent on cache storage, which is on by default.

Here are the config settings are listed that fully disables the usage of cache storage.

* Cache Storage - Disabled the cache storage mount, and the manifest changes needed. Property `switch.cache_storage_enabled = 0`
* Live Update - Manages DLC. Property `liveupdate.enabled = 0`
* Http Cache - Caches downloaded files. Property `network.http_cache_enabled = 0`
* Resource Http Cache - Caches hot reloaded resource files. Property `resource.http_cache = 0`

Since especially the http cache feature is useful during development, with hot reloading, it might be a good idea to 
put these options in a `release_switch.properties` file that can be appended to the regular `game.project` file when bundling.