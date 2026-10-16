# Ogg build tools

`build_ext` builds libogg 1.3.5, liboggz 1.1.1, and `oggz-validate` from source
for the five desktop platforms. Cross-builds also build the host tools. The
libraries install into `$DYNAMO_HOME/ext/lib/<platform>` and the validator into
`ext/bin/<platform>`. No prebuilt Ogg package is required.

Bob's `tools.json` packs them together in `libexec/<platform>`. Bob Light includes
the two libraries. The Editor's `ResourceUnpacker` extracts them from Bob.
CI caches the installation with the other source dependencies. Engine jobs
archive the native tools; `sync_archive` supplies the other desktop platforms
when packaging full Bob, preserving locally built host/target tools.

`OggUtil.stripMetadata(byte[])` loads the libraries through Bob's extraction
cache and JNA. Each call owns its native reader and writer, allowing parallel
builds. It strips the vendor and comments from a single Vorbis stream, retaining
the identification/setup packets and copying audio pages without re-encoding.
Only their sequence numbers and checksums change when the header page count
changes. Opus streams, including those named `.ogg`, are identified from their
first packet and returned byte-for-byte unchanged after Ogg framing validation.
Chained, multiplexed, or unsupported codec streams are rejected.

The CMake build sets relative loader paths so the libraries work both in the SDK
and after extraction beside each other. Windows builds use the upstream export
lists and getopt implementation. The installed Windows validator gets copies of
its DLLs in `ext/bin/<platform>` as well.

## Upstream sources

`package/` contains unmodified source files from the official Xiph releases:

| Source | SHA-256 of upstream archive |
| --- | --- |
| [libogg 1.3.5](https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.xz) | `c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705` |
| [liboggz 1.1.1](https://downloads.xiph.org/releases/liboggz/liboggz-1.1.1.tar.gz) | `6bafadb1e0a9ae4ac83304f38621a5621b8e8e32927889e65a98706d213d415a` |

For libogg, retain `COPYING`, `src/{bitwise.c,framing.c,crctable.h}`,
`include/ogg/{ogg.h,os_types.h,config_types.h.in}`, and `win32/ogg.def`.
For liboggz, retain `COPYING`, `README`, the C sources and headers in
`src/liboggz`, the public headers in `include/oggz` (using the `.in` template for
`oggz_off_t_generated.h`), `src/tools/{oggz-validate.c,oggz_tools.c,oggz_tools.h,oggz_tools_dirac.h}`,
and `win32/{attgetopt.c,getopt.h,liboggz.def}`.

Defold's build and generated-header configuration live outside `package/`.
Keep any future upstream source changes in a patch, as described in the
[external dependency guide](../README.md#modifications).
