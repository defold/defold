# SDK-free DXC prototype

This standalone experiment exercises SM6.0 compilation, reflection, and root
signature handling without FXC, D3DCompile, D3DReflect, or D3D12 serialization
APIs. It does not change the production shader pipeline.

DXC is installed through `install_ext`: Windows uses Defold's prebuilt package,
while Linux and macOS build from source. See the
[installation and rebuild instructions](../../../../../external/dxc/README.md)
to run this prototype against those installed binaries.

`run.py` requires Python 3 and DXC. It compiles vertex, pixel, and compute
fixtures, checking CBV/SRV/sampler/UAV bindings, register spaces, a texture array,
and constant buffer offsets and size. It also:

- Compiles a shared root-signature description covering all three stages.
- Checks compatibility with each shader and rejects an incompatible signature.
- Attaches and extracts the signature, checking its serialized payload.
- Strips reflection while preserving DXIL and the root signature.
- Optionally compares normalized reflection and root-signature hashes from
  another host.

The command-line checks read DXC's disassembly for reflection assertions.
`reflect.cpp` separately proves the production-relevant native API:
`IDxcUtils::CreateReflection` on the binary `-Fre` output, followed by
`ID3D12ShaderReflection` resource, constant-buffer, member, and thread-group
queries. It uses open-source headers, not the Windows SDK. Its harness currently
targets Unix hosts.

## Dependencies

Tested with [DXC v1.9.2609](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609):

| Official release archive | SHA-256 |
| --- | --- |
| `dxc_2026_09_29.zip` | `ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1` |
| `linux_dxc_2026_09_28.x86_x64.tar.gz` | `96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1` |

Native reflection additionally needs
[DirectX-Headers commit 980971e835876dc0cde415e8f9bc646e64667bf7](https://github.com/microsoft/DirectX-Headers/tree/980971e835876dc0cde415e8f9bc646e64667bf7),
the submodule revision pinned by this DXC release. Do not substitute the latest
headers: v1.619.5's Unix stubs conflict with DXC's `WinAdapter.h` definitions.
No dependencies are vendored or automatically downloaded by the prototype.

## Run

From the repository root, after extracting the relevant DXC archive:

```powershell
python engine/shaderc/src/test/dxc_prototype/run.py `
  --dxc C:/path/to/dxc/bin/x64/dxc.exe --out tmp/dxc-windows
```

```sh
python3 engine/shaderc/src/test/dxc_prototype/run.py \
  --dxc /path/to/dxc/bin/dxc --out tmp/dxc-linux \
  --compare tmp/dxc-windows/report.json
```

The comparison file is optional; copy it from the other host if necessary.
Root signatures default to version 1.0 to match the engine. Use
`--root-version 1_1` on both hosts to check version 1.1.
Outputs, diagnostic dumps, command exit codes, and negative-case diagnostics
remain in the selected output directory.

Build the native Linux probe (C++11 is required by the external DXC headers):

```sh
g++ -std=c++11 -Wall -Wextra \
  -I/path/to/dxc/include \
  -I/path/to/DirectX-Headers/include/directx \
  -I/path/to/DirectX-Headers/include/wsl/stubs \
  engine/shaderc/src/test/dxc_prototype/reflect.cpp -ldl -o /tmp/dxc-reflect

python3 engine/shaderc/src/test/dxc_prototype/run.py \
  --dxc /path/to/dxc/bin/dxc --out tmp/dxc-linux \
  --reflect /tmp/dxc-reflect --dxcompiler /path/to/dxc/lib/libdxcompiler.so
```

The probe can also inspect an individual `.reflection` file:

```sh
/tmp/dxc-reflect /path/to/dxc/lib/libdxcompiler.so tmp/dxc-linux/ps.reflection
```

## Results and limits

Validated on 2026-10-20:

- Windows x64 and native Linux x64 under WSL: all command-line checks pass for
  root-signature versions 1.0 and 1.1. Reflected layouts match across hosts;
  serialized root-signature payloads are byte-identical.
- Linux native reflection: all three shader stages pass.
- The Linux tests load no Windows SDK or Direct3D runtime libraries.

The official Linux binary does not run with this machine's Ubuntu 20.04
glibc 2.31. It reports requirements including GLIBC_2.38 and GLIBCXX_3.4.29.
Testing used libraries extracted into an isolated directory from the official
Ubuntu base 24.04.3 amd64 archive (SHA-256
`6bc2cde3930ad088b3bb46fa45279e96d25bc3810f209850ecbe4722711874f9`),
without upgrading the WSL installation. Both DXC and the native probe were
launched through that directory's `ld-linux-x86-64.so.2 --library-path ...`.
The runner accepts wrapper executables for this purpose. Shipping tools will
need a deliberate Linux runtime baseline or a DXC build targeting an older one.

The subsequent [source-build validation](../../../../../external/dxc/README.md)
passes the same checks on Windows and Ubuntu 20.04 using the original system
libraries, including native Linux reflection and comparison of both
root-signature versions. The isolated newer libraries above are only needed
for reproducing the upstream prebuilt-binary experiment.

macOS, ARM hosts, GPU execution, engine rendering, and console toolchains have
not been tested here. The root-signature fixture is deliberately explicit:
this does not yet implement the engine's arbitrary layout merge, register
remapping, visibility rules, or descriptor offsets.

## Production follow-up

The experiment supports replacing the Windows SDK shader build operations
with DXC. Keep a platform-neutral binding/layout representation, merge it
before serialization, emit a root-signature description, and compile that
description with DXC. Preserve root-parameter ordering and resource mappings
expected by the runtime. This avoids needing D3D12 deserialization to merge
already serialized signatures.

Use native reflection in shaderc rather than parsing diagnostic text. Keep
compiler selection separate from layout generation so platform-specific
compiler backends can consume the shared layout. Their binary compatibility
and runtime behavior require separate platform validation.

Move generated shaders directly to SM6.0; no SM5.1 compatibility reader or
dual-output transition is needed. Replace the existing production path and
invalidate generated artifacts/caches as appropriate when integrating it.
First validate representative engine-generated HLSL and the Bob shader tests,
then runtime pipelines on supported desktop and console targets.
