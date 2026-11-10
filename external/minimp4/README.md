# minimp4

`minimp4.h` and `LICENSE` from
https://github.com/lieff/minimp4/tree/2a3014a7753cfcba4442adeb469431967238fe6a.

Upstream header SHA-256 (before applying the patch):
`9d122076cc23e5f4a7560ce4056c7d84484a8b8bf094d27c8f652f11fc725ce7`.

The Linux recorder compiles the MP4 muxer directly. Tests use its demuxer to
inspect the recorded track and pass H.264 samples to OpenH264 for decoding.

Local changes are recorded in [defold.patch](defold.patch) and already applied
to the checked-in header. The patch accesses H.264 bitstream buffers as bytes,
using `memcpy` for word loads and stores so NAL payloads do not require two- or
four-byte alignment. The modified bit reader and writer are marked with
`DEFOLD BEGIN` / `DEFOLD END` comments.

To reproduce the checked-in header, copy `minimp4.h` from the upstream revision
above into this directory and run:

```sh
patch -p1 < defold.patch
```
