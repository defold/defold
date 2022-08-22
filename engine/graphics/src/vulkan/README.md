# Vulkan

## Updating the packages

The Vulkan packages are not built from source so update the packages must be done manually.
The current procedure is to first go to the official SDK site (https://vulkan.lunarg.com/sdk/home) and
download the bundles for each available platform. Copy the appropriate library files from within the SDK and make
a tar ball as usual per platform. Make a common package for the headers - these can usually be taken from any of the SDKs.
To see what SDK version the engine is using, look under the `packages` folder in ther root directory.

* Android - the Vulkan SDK is distributed with the NDK, so it should already be available on that platform.

## Enabling validation Layers

To enable validation layers, set the environment variable `DM_VULKAN_VALIDATION=1`.
This may not be a guarantee that validation is supported by your host platform, but
the flag needs to be set for the graphics system to poll for support.

### OSX

By default, we link the engine with a static version of the MoltenVK library, which doesn't
have any support for validation layers. To get validation layers to work on OSX and iOS platforms,
you need to copy the vulkan dylib files from the SDK (typically resides under `<path-to-sdk>/macOS/lib/)`
to the `$DYNAMO_HOME/tmp/share/lib/x86_64-macos` directory, the `engine/engine/wscript` will link with
the dylib library when `DM_VULKAN_VALIDATION` is set. You don't need to copy the actual
validation layer libraries, since they will be automatically picked up by the loader from the paths
specified in the manifest files.

For the loader to work correctly, you need to set a new environment variables to various
places in the SDK. Currently, this has only been tested on OSX but the process should be similar
for the other platforms.

```
# Set the Vulkan SDK to the root of the SDK folder
export VULKAN_SDK=<path-to-vulkan-sdk>

# This outputs everything the loader does, from searching for layers to ICDs
export VK_LOADER_DEBUG=all

# This lets the loader know where the ICD's are located. By default it searches
# for places like ~/.local/share/vulkan but it's likely better to use the SDK files.
export VK_ICD_FILENAMES=$VULKAN_SDK/macOS/etc/vulkan/icd.d/MoltenVK_icd.json

# Location where the loader should look for the layers. The folder contains a bunch
# of JSON manifests that let's the loader know of the layers that are available.
# The layer code is located in a bunch of dynamic libraries that gets loaded
# automatically by the vulkan loader if we link against libvulkan and not libMoltenVK
export VK_LAYER_PATH=$VULKAN_SDK/macOs/etc/vulkan/explicit_layer.d

# Make sure the linker can find the layer dylibs
export LD_LIBRARY_PATH=$VULKAN_SDK/macOS/lib/
```

## Windows

To enable validation for Windows, you need to install the Vulkan SDK at (https://vulkan.lunarg.com/sdk/home#windows).
Make sure to install the same version as we are currently using in the engine, although it will probably work with a newer
version aswell. The SDK will add entries into the registry so that the loader can find the validation layer libraries from
the SDK.
