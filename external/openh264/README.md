# OpenH264

Unmodified codec sources from Cisco OpenH264 **2.6.0**:
https://github.com/cisco/openh264/releases/tag/v2.6.0

Source archive SHA-256:
`558544ad358283a7ab2930d69a9ceddf913f4a51ee9bf1bfb9e377322af81a69`.
Only `codec/`, `LICENSE`, and the upstream `README.md` are vendored, excluding
the unused IDE and sample projects under `codec/build/` and
`codec/processing/build/`.

`build_ext` builds this static library only for x86_64-linux and arm64-linux.
Linux recording uses the encoder; tests also use the decoder to verify MP4
contents. Unreferenced decoder objects are omitted when linking the engine.
ARM64 uses NEON assembly. x86_64 uses dispatched assembly when NASM is available,
and the portable C++ kernels otherwise. No upstream source patches are applied.
