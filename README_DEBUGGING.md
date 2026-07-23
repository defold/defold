# Debugging

## Get the \_crash / .dmp files

### Android

The adb output says where it is located (different location on different devices)

If the app is [debuggable](), you can get the crash log like so:

	$ adb shell "run-as com.defold.adtest sh -c 'cat /data/data/com.defold.adtest/files/_crash'" > ./_crash

If the device has `su`, you can try these commands (The device likely has to be rooted):

	$ adb pull /data/data/com.defold.examples/files/_crash ./_crash

If those fail, try the [Device File Explorer](https://developer.android.com/studio/debug/device-file-explorer) in Android Studio.


### iOS

## Get engine version and SHA1 from binary

### Android

	$ objdump -s -j .rodata blossom_45_0_1/lib/armeabi-v7a/libblossom_blast_saga.so | grep -e "1\.2\."

Look for "1.2.XXX", which is usually the first string, followed by the SHA1

### iOS + OSX

    $ strings test.app/test | grep -e "1\.2\.[0-9]+"


### Get engine SHA1

iOS + OSX:

    $ strings pewpew/Payload/Pewpew.app/Pewpew | grep -E "^[0-9a-f]{40}"


## Symbolicate crashes


### Android

1. Download the engine:

	$ wget http://d.defold.com/archive/<sha1>/engine/armv7-android/dmengine.apk

1. Alternatively, get it from your build folder

	$ ls <project>/build/<platform>/[lib]dmengine[.exe|.so]

1. Unzip to a folder:

	$ unzip dmengine.apk -d dmengine_1_2_105

1. Find the callstack address

	E.g. in the non symbolicated callstack on Crash Analytics, it could look like this

	`#00 pc 00257224 libblossom_blast_saga.so`

	Where *00257224* is the address

1. Resolve the address

```
    # 32 bit
    $ arm-linux-androideabi-addr2line -C -f -e dmengine_1_2_105/lib/armeabi-v7a/libdmengine.so <address>

    # 64 bit
    $ aarch64-linux-android-addr2line -C -f -e build/default/extension-push/extension-push.apk.symbols/lib/arm64-v8a/libextensionpush.so <address>
```

### iOS

1. Download the symbols (.dSYM)

1. If you're not using Native Extensions, download the vanilla symbols:

	$ wget http://d.defold.com/archive/<sha1>/engine/arm64-ios/dmengine.dSYM

1. If you are using Native Extensions, the server can provide those for you (pass "--with-symbols" to bob.jar)

	$ unzip <project>/build/arm64-ios/build.zip

	it will produce a Contents/Resources/DWARF/dmengine

1. Symbolicate using load address

	For some reason, simply putting the address from the callstack doesn't work (i.e. load address 0x0)

	$ atos -arch arm64 -o Contents/Resources/DWARF/dmengine 0x1492c4

	Neither does specifying the load address directly

	$ atos -arch arm64 -o MyApp.dSYM/Contents/Resources/DWARF/MyApp -l0x100000000 0x1492c4

	Adding the load address to the address works:

	$ atos -arch arm64 -o MyApp.dSYM/Contents/Resources/DWARF/MyApp 0x1001492c4
	dmCrash::OnCrash(int) (in MyApp) (backtrace_execinfo.cpp:27)

### macOS

Same as iOS, except you don't need to specify the <arch>

#### Other tools

UUID:

	# Check the UUID of an executable
	$ dwarfdump --uuid dmengine
	# Check the UUID of the debug symbols
	$ dwarfdump --uuid dmengine.dSYM/Contents/Resources/DWARF/dmengine


### HTML5

#### Native debugging in the browser (DWARF / source maps)

HTML5 builds can be debugged as native C/C++ code directly in the browser.
Bundle with the debug variant and symbols:

	$ java -jar bob.jar --platform=wasm-web --architectures=wasm-web --variant=debug --with-symbols resolve build bundle

With `--with-symbols` the bundle contains the wasm debug info next to the engine:

* `<Project>.wasm.debug.wasm` — the DWARF debug info (used by Chrome)
* `<Project>.wasm.map` — a wasm source map with inlined sources (used by Firefox)

and `dmloader.js` switches to URL-preserving streaming instantiation of the wasm
(browser devtools resolve the debug info relative to the URL of the wasm module,
so a module instantiated from an ArrayBuffer cannot be debugged).

The debug info comes from the engine link step:

* Vanilla bundles use the sidecars published for the *debug* variant of the official
  engines (release/headless variants are not published with wasm debug info).
  Locally built engines produce them for both `--opt-level < 2` (full debug info)
  and optimized builds (line tables only) — see `engine/engine/src/wscript`.
* Extender (native extension) builds link with `-gseparate-dwarf -gsource-map`
  when bob sends `withSymbols` (see `withSymbolsLinkFlags` in `share/extender/build_input.yml`),
  so extension code is debuggable too. Note: the server does not embed sources
  into the map, so in Firefox extender-built engines show the file tree without
  content; Chrome + DWARF is unaffected.

Serving requirements:

* Serve `.wasm` with `Content-Type: application/wasm` (required for streaming
  instantiation; `python3 -m http.server` works).
* The sidecars must be served from the same directory as the `.wasm`.
* The `_pthread` architecture additionally needs the COOP/COEP headers
  (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`).
* Do not deploy the sidecars to production — the DWARF file contains the full
  debug info (and code/data sections) and can be 50-200 MB; the inline source
  map contains the sources.

Chrome:

1. Install the [C/C++ DevTools Support (DWARF)](https://chromewebstore.google.com/detail/cc++-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb) extension.
2. Open DevTools before loading the page. The C/C++ sources appear in
   Sources -> Page under the wasm module; breakpoints, stepping, and (at
   `--opt-level=0/1`) local variable inspection work.
3. For engine builds from CI the DWARF source paths are repo-relative
   (e.g. `engine/dlib/src/dlib/hash.cpp`, comp dir `engine/<lib>` for waf builds,
   SDK headers under `defoldsdk/`). Use the extension options -> path substitution
   to map these prefixes to a local Defold checkout. Emscripten system libraries
   appear under `/emsdk/emscripten`.
4. Extension builds compile on the Extender server under the job directory;
   pass `debugSourcePath` (top-level appmanifest context) to control the
   compilation dir, and map `extensions/` -> your project in path substitutions.

Firefox:

* Firefox reads the wasm source map. Engine builds embed the sources into the
  map (`sourcesContent`, via `build_tools/embed_wasm_sourcemap_sources.py`), so
  files and line breakpoints/stepping work. Note that wasm source maps carry no
  scope/type information — there is no variable inspection in Firefox; use
  Chrome + DWARF for the full experience.
* The sources usually appear only after a second reload with devtools open, and
  a breakpoint set on a C/C++ line is often not armed until the debugger has
  paused once: hit pause and resume, and the breakpoints start hitting. Firefox
  installs source mapped wasm breakpoints lazily; this is a devtools quirk, not
  a problem with the debug info (Chrome does not need it).
* For Extender builds the engine libraries are prebuilt, so engine files may show
  without content; extension sources are embedded.
* Firefox always logs one `Source map error: Error: URL constructor: is not a
  valid URL` for a module named `wasm:<page>/dmloader.js line NNN >
  WebAssembly.Module`. That is not the engine: it is the eight byte module
  `EngineLoader.isWASMSupported` compiles from a byte array to detect wasm
  support (a URL-less module devtools cannot map). The engine module appears
  separately as `wasm:<page>/<Title>.wasm` and is the one to look for.

Safari:

* Native C/C++ debugging is not available, and no Web Inspector setting enables
  it. JavaScriptCore does parse the `sourceMappingURL` section of the wasm
  module, but Web Inspector never requests the map: its source map downloads are
  driven by scripts and by stylesheet/script network resources, not by wasm.
  Verified — Safari does not fetch the `.wasm.map` from the custom section, nor
  when the `.wasm` is served with an explicit `SourceMap:` response header.
  There is no DWARF support either (the Chrome C/C++ extension is a Chrome
  DevTools extension with no Safari counterpart). You get the wasm
  disassembly, the console, and JS level stacks. Use Chrome to debug engine
  code, and Safari only to reproduce Safari specific issues — the
  `.js.symbols` map still symbolicates callstacks there.

Browsers identify the engine module by the URL it was fetched from, so if
the module shows up as `wasm://wasm/...` or named after `dmloader.js`, the
streaming instantiation was not used and no debug info will be attached — check
that the `.wasm` is served with `Content-Type: application/wasm`.

#### Symbolicating a callstack with the symbol map

1. Download the engine:

	$ wget http://d.defold.com/archive/<sha1>/engine/wasm-web/dmengine.js

1. Download the symbols

	$ ... dmengine.js.symbols

1. Match the callstack with the symbols
