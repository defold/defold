# Complying with licenses

Defold is created and distributed under the developer-friendly Defold License. The Defold License is derived from the popular Apache 2.0 license. The license in its entirety can be [read here](/LICENSE.txt) with additional information to help you understand the [difference from the standard Apache 2.0 license here](https://defold.com/license/).

Defold itself contains software written by third parties. Some of the software is used in the Defold editor and command line tools ([learn more](/TOOLS_LICENSES.md)) and some of the software is used in the Defold engine. The software included in the Defold engine will be included in the games and applications created using Defold and require the inclusion of their respective license in derivative work.

**If you use Defold to create games and applications it is your responsibility to include the required licenses in your work.**


## How to include required licenses in your work

Some ways in which you can include required licenses in your work:

* Show the license and notice at the end of a credits screen if your application has one
* Show the license and notice from a dedicated license screen or popup in your application
* Print the license and notice to an output log when your application starts
* Put the license and notice in a text file and include it with the distribution of your application
* Put the license and notice in a printed manual included with your application


## Required license inclusion

The following software has licenses which require inclusion of their respective license in derivative work:

  * Defold License 1.0
    * [Defold](/NOTICE)
  * Apache 2.0
    * [mbedTLS](/licenses/NOTICE-mbedtls) - Used when doing HTTPS requests or creating SSL sockets. Included in all builds of Defold.
    * [Basis Universal](/licenses/NOTICE-basisuniversal) - Used when compressing textures.
    * [Remotery](/licenses/NOTICE-remotery) - Used during development of your game. Not included in the release builds of Defold.
  * MIT
    * [jctest](/licenses/NOTICE-jctest) - Used when running unit tests in the engine. Not included in any builds.
    * [Lua](/licenses/NOTICE-lua) - Used in HTML5 builds.
    * [LuaCJson](/licenses/NOTICE-luacjson) - Used for encoding lua tables to JSON. Used on all platforms.
    * [LuaJIT](/licenses/NOTICE-luajit) - Used on all platforms except HTML5.
    * [LuaSocket](/licenses/NOTICE-luasocket) - Used for socket communication from Lua. Included on all platforms.
    * [microsoft_craziness.h](/licenses/NOTICE-microsoft_craziness) - Used when running unit tests in the engine. Not included in any builds.
    * [XHR2](/licenses/NOTICE-xhr2) - Used in HTML5 builds.
    * [xxtea-c](/licenses/NOTICE-xxtea) - Used internally in the engine to obfuscate/encode Lua source code. Included in all builds of Defold.
    * [cgltf](/licenses/NOTICE-cgltf) - Used when loading models in glTF format. Included in all builds.
    * [lipo](https://github.com/konoui/lipo) - Used when building bundles for macOS and iOS. Included in all Editor/bob.jar builds.
    * [miniz](https://github.com/richgel999/miniz) - Rich Geldrich
    * [zip](https://github.com/kuba--/zip) - Kuba Podgórski
    * [SampleRateConverter] (https://github.com/zephray/SampleRateConverter/tree/master) - Used off line to generate polyphase filter coefficients for sound system
  * Simplified BSD license (2-clause license)
    * [LZ4](/licenses/NOTICE-lz4) - Used internally by the engine to read game archives. Included in all builds.
  * BSD 2.0 license (3-clause license)
    * [vpx/vp8](/licenses/NOTICE-vpx-vp8) - Used by the game play recorder. Only included in debug builds by default, but can be added [using an app manifest](https://defold.com/manuals/project-settings/#app-manifest).
    * [Tremolo](/licenses/NOTICE-tremolo) - Used for decoding of Ogg sound files. Not used on Nintendo Switch, Win32 or HTML5. Included in all builds, but can be excluded [using an app manifest](https://defold.com/manuals/project-settings/#app-manifest)
    * [Sony Vector Math library](/licenses/NOTICE-vecmath) - Used internally by the engine. Included in all builds.
  * LGPL 2.0
    * [OpenAL](/licenses/NOTICE-openal) - Used for sound playback on all platforms except Android. Can be excluded [using an app manifest](https://defold.com/manuals/project-settings/#app-manifest)


## Optional license inclusion

The following third party software has licenses which does not require inclusion of their respective license in derivative work:

  * Public Domain
    * stb_image - Sean Barrett
    * stb_truetype - Sean Barrett
    * stb_vorbis - Sean Barrett
  * ZLib
    * zlib - Jean-loup Gailly and Mark Adler
    * Box2D - Erin Catto
    * Bullet Physics - Erwin Coumans
    * GLFW - Marcus Geelnard, Camilla Löwy
    * bindgen (Sokol) - Andre Weissflog

NOTE: The Zlib license encourages attribution but does not require inclusion of the license.
