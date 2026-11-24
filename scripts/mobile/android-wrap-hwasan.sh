#!/system/bin/sh
# Android 14+ supplies the HWASan runtime when the linker sees LD_HWASAN.
# https://developer.android.com/ndk/guides/hwasan
LD_HWASAN=1 exec "$@"
