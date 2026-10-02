# LuaJIT

LuaJIT is built from the bundled upstream revision
`3e223cb8a41cff3b92931acb846af7b67a5d2537`, with `defold.patch` renaming its
public Lua symbols to avoid conflicts with other libraries.

Build and install it through the regular Defold platform setup:

```sh
./scripts/build.py --platform=arm64-macos install_ext
```

`build_ext` rebuilds source dependencies without reinstalling packages. It
builds host dependencies first when cross-compiling. LuaJIT is included on
native targets; web targets use the engine's Lua 5.1 implementation.

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

The runtime inherits `defold_sdk` compiler, SDK and platform settings. Native
CMake host tools generate the VM and headers using the target compiler's
LuaJIT architecture settings. Mobile targets keep JIT disabled and dual-number
mode; `armv7-android` also disables GC64 and uses internal unwinding. Console
targets disable JIT and use the system allocator. Xbox keeps FFI enabled;
other console targets disable FFI. A 32-bit target requires a matching 32-bit host
toolchain, such as Linux with `libc6-dev-i386` and `gcc-multilib` installed.
See the [upstream cross-compilation requirements](https://luajit.org/install.html#cross).

To update, change the revision in `version.sh` and `CMakeLists.txt`, bundle
the corresponding upstream archive, and regenerate `defold.patch` with
`make-patch.sh`. LuaJIT is installed directly and no package tarballs are built.
