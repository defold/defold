# LuaJIT

LuaJIT is built from the bundled upstream revision
`63c8f1a9b479ba5518cf6a170ff7729178bc3cfd`. The patches are applied in order:

1. `defold.patch` renames public Lua symbols to avoid conflicts with other libraries.
2. `defold-wasm.patch` adds the Wasm interpreter and its regression tests.

Both patch hashes participate in the extracted-source signature, so changing
either patch causes the source tree to be regenerated. The Wasm additions do
not replace the native assembler backends.

Build and install it through the regular Defold platform setup:

```sh
./scripts/build.py --platform=arm64-macos install_ext
```

`build_ext` rebuilds source dependencies without reinstalling packages. It
builds host dependencies first when cross-compiling. LuaJIT is included on
native targets and both `wasm-web` and `wasm_pthread-web`. MemorySanitizer
builds retain the engine's Lua 5.1 implementation.

To build just LuaJIT from the repository root, with the platform SDK installed:

```sh
cmake -S external/luajit -B external/luajit/build/arm64-macos -G Ninja \
  -DTARGET_PLATFORM=arm64-macos -DBUILD_TESTS=OFF \
  -DDEFOLD_BUILD_HOME="$PWD" -DDEFOLD_SDK_ROOT="$DYNAMO_HOME"
cmake --build external/luajit/build/arm64-macos
cmake --install external/luajit/build/arm64-macos
```

The static library installs into `ext/lib/<platform>` and headers into
`ext/include/luajit-2.1`. Host builds install Lua modules into
`ext/share/luajit/jit`; cross-builds preserve these host modules. When building a
cross-target directly with CMake, build and install the host first.
Desktop targets also install `ext/bin/<platform>/luajit-64` (with `.exe` on
Windows), which Bob uses to compile both 32-bit and 64-bit bytecode.

The runtime inherits `defold_sdk` compiler, SDK and platform settings. CMake
host tools generate the VM and headers using the target compiler's
LuaJIT architecture settings. Mobile targets keep JIT disabled and dual-number
mode; `armv7-android` also disables GC64 and uses internal unwinding. Console
targets disable JIT and use the system allocator. Xbox keeps FFI enabled;
other console targets disable FFI. A 32-bit target requires a matching 32-bit host
toolchain, such as Linux with `libc6-dev-i386` and `gcc-multilib` installed. On
Windows, CMake selects the x86 compiler and SDK libraries for these generators.
On macOS, the generators are built as 32-bit WebAssembly and run in Node.
`./scripts/build.py --platform=armv7-android install_sdk` also installs the Web
SDK needed for these generators. A local Emscripten installation can be selected
through `EMSDK` or `EMSCRIPTEN`. `./scripts/build.py check_sdk --platform=armv7-android`
verifies the Android toolchain and, on macOS, the Emscripten compiler and Node
needed by the generators. The runtime still uses the Android SDK and its platform
flags.
See the [upstream cross-compilation requirements](https://luajit.org/install.html#cross).

To update, change the revision in `version.sh` and `CMakeLists.txt`, bundle
the corresponding upstream archive, and regenerate `defold.patch` with
`make-patch.sh`. Rebase `defold-wasm.patch` against the symbol-renamed source,
then verify that both patches apply in that order. LuaJIT is installed directly
and no package tarballs are built.

## Web runtime and bytecode

Both web targets link `luajit-5.1`, using the portable C interpreter with JIT,
FFI and profiling disabled, dual-number mode, the system allocator and Defold's
16-byte userdata alignment. The generated headers use the Wasm target layout;
there is no assembler VM on these targets. The native assembler backends remain
in use on other platforms.

Bob packages 32-bit, non-GC64 LuaJIT bytecode for web by default, including
`.lua`, `.script`, `.gui_script` and `.render_script` resources. It uses the
native host's rebuilt `luajit-64` and matching `jit` modules at the pinned
revision, with the existing `-W` bytecode option. A Wasm host compiler backend
is not required. The upstream version-2 bytecode format is unchanged; GC64
(`-X`) bytecode is incompatible with the Wasm interpreter.

The explicit `--use-uncompressed-lua-source` option still packages source.
Default bytecode retains the existing encryption and archive-compression flags.
Rebuild Bob after installing the updated host compiler and modules, and rebuild
web content together with the engine: older Lua 5.1 web engines cannot load
LuaJIT bytecode.

CMake and extension builds enable Wasm tail calls and `SUPPORT_LONGJMP=wasm`.
The browser minimums are Chrome 112, Firefox 121 and Safari 18.2, matching
[Wasm tail-call support](https://webassembly.org/features/?categories=browsers).

### Remaining integration limit

Emscripten 4.0.6's Asyncify pass rejects Wasm tail calls at link time. The current
`WITH_WEBGPU` configuration enables Asyncify and therefore needs a compatible
suspension/dispatch solution before it can use this interpreter. The normal
web and pthread libraries and their bytecode paths can be built and tested
without Asyncify.
