#!/system/bin/sh
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

# Android 14+ supplies the HWASan runtime when the linker sees LD_HWASAN.
# https://developer.android.com/ndk/guides/hwasan
LD_HWASAN=1 exec "$@"
