# Vulkan

* Current SDK version of Vulkan packages is 1.1.108

## Updating the packages

The Vulkan packages are not built from source so update the packages must be done manually.
The current procedure is to first go to the official SDK site (https://vulkan.lunarg.com/sdk/home) and
download the bundles for each available platform. Copy the appropriate library files from within the SDK and make
a tar ball as usual per platform. Make a common package for the headers - these can usually be taken from any of the SDKs.

Note: For Android, the Vulkan SDK is distributed with the NDK, so it should already be available on that platform.
