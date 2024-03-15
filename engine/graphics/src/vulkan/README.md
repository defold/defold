# Vulkan

## Updating the packages

The Vulkan packages are not built from source so update the packages must be done manually.
The current procedure is to first go to the official SDK site (https://vulkan.lunarg.com/sdk/home) and
download the bundles for each available platform. Copy the appropriate library files from within the SDK and make
a tar ball as usual per platform. Make a common package for the headers - these can usually be taken from any of the SDKs.
To see what SDK version the engine is using, look under the `packages` folder in ther root directory.

* OSX - NOTE! The osx packages are a mix of custom built moltenvk packages + vulkan sdks, this is because we require this patch for some of our extensions to work correctly (https://github.com/jpd002/MoltenVK/commit/898553af81e3ee24e82d5c654c15776c3c1b4b78). To update the package, MoltenVK needs to be built manually together with this patch. At some point we should automate this process, but for now it needs to be done manually.
* Android - the Vulkan SDK is distributed with the NDK, so it should already be available on that platform.

## Validation Layers

### OSX

By default, we link the engine with a static version of the MoltenVK library, which doesn't
have any support for validation layers. To get validation layers to work on OSX and iOS platforms,
you need to copy the vulkan dylib files from the SDK (typically resides under `<path-to-sdk>/macOS/lib/)`
to the `$DYNAMO_HOME/tmp/share/lib/x86_64-macos` directory, the `engine/engine/wscript` will link with
the dylib library when `--with-vulkan-validation` is passed to `wawf`. You don't need to copy the actual
validation layer libraries, since they will be automatically picked up by the loader from the paths
specified in the manifest files.

For the loader to work correctly, you need to set a new environment variables to various
places in the SDK. Currently, this has only been tested on OSX but the process should be similar
for the other platforms. To run the engine in "validation mode", use the `scripts/macos/macos-vulkan-validation-layers.sh` script.

```
# from the defold base directory:
./scripts/macos/macos-vulkan-validation-layers.sh arm64-macos
```

## Windows

To enable validation for Windows, you need to install the Vulkan SDK at (https://vulkan.lunarg.com/sdk/home#windows).
Make sure to install the same version as we are currently using in the engine, although it will probably work with a newer
version aswell. The SDK will add entries into the registry so that the loader can find the validation layer libraries from
the SDK.
