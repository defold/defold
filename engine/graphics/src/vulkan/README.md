# Vulkan

## Updating the packages

The [Vulkan packaging script](../../../../share/ext/vulkan/build_vulkan.sh) pins the loader, headers and validation layers. It builds them from source and writes a common header archive and a platform archive to `share/ext/vulkan/package/`. For example, on Apple silicon, run the following in the Defold build shell from the repository root.

```sh
cd share/ext
./build.sh vulkan arm64-macos
```

The standard macOS and iOS engine builds link a static MoltenVK library. Follow the [MoltenVK package instructions](../../../../share/ext/moltenvk/README.md) to update the macOS, iOS device or iOS simulator library. Android uses Vulkan from the NDK.

## Validation layers

### macOS

Enabling Vulkan validation switches the macOS engine to the dynamic Vulkan loader. Install the loader under `$DYNAMO_HOME/ext/lib/<platform>/` before rebuilding. For Apple silicon, the repository includes the matching archive. Run from the repository root in the Defold build shell.

```sh
tar -xf packages/vulkan-v1.4.307-arm64-macos.tar.gz -C "$DYNAMO_HOME/ext"
./scripts/build.py build_engine --platform=arm64-macos -- --with-vulkan --with-vulkan-validation
```

Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) for its validation layers. Set `VULKAN_SDK` to the SDK directory containing `macOS/lib`. From a built game's directory, run `/path/to/defold/scripts/macos/macos-vulkan-validation-layers.sh arm64-macos`. The [launcher](../../../../scripts/macos/macos-vulkan-validation-layers.sh) configures the library path and starts the engine with `--graphics-adapter=vulkan --use-validation-layers`.

### Windows

Install the [Windows Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) matching the version used by the engine. The SDK registers its validation layers with the system loader. Start the Vulkan engine with `--use-validation-layers` to enable them.
