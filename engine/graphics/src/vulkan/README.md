# Vulkan

## Enabling validation Layers

By default, we link the engine with a static version of the MoltenVK library, which doesn't
enable validation layers. To get validation layers to work, you need to copy the vulkan dylib files
from the SDK to the $DYNAMO_HOME/tmp/share/lib/x86_64-darwin directory and link with vulkan instead.
You don't need to copy the validation layer libraries, they should be picked up automatically
by the loader.

For the loader to work correctly, you need to set a bunch of environment variables to various
points in the SDK, which is described per platform in the next section.

### OSX

```
# This outputs everything the loader does, from searching for layers to ICDs
export VK_LOADER_DEBUG=all

# This lets the loader know where the ICD's are located. By default it searches
# for places like ~/.local/share/vulkan but it's likely better to use the SDK files.
export VK_ICD_FILENAMES=$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json

# Location where the loader should look for the layers. The folder contains a bunch
# of JSON manifests that let's the loader know of the layers that are available.
# The layer code is located in a bunch of dynamic libraries that gets loaded
# automatically by the vulkan loader if we link against libvulkan and not libMoltenVK
export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d

# Make sure the linker can find the layer dylibs
export LD_LIBRARY_PATH=$VULKAN_SDK/macOS/lib/
```

TODO add rest of supported platform info here!

## Updating the packages

* Current SDK version of Vulkan packages is 1.1.108

The Vulkan packages are not built from source so update the packages must be done manually.
The current procedure is to first go to the official SDK site (https://vulkan.lunarg.com/sdk/home) and
download the bundles for each available platform. Copy the appropriate library files from within the SDK and make
a tar ball as usual per platform. Make a common package for the headers - these can usually be taken from any of the SDKs.

* Android - the Vulkan SDK is distributed with the NDK, so it should already be available on that platform.
