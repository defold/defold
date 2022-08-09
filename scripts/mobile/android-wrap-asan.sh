#!/system/bin/sh
echo -----------------------------------------------------------------------
echo DEFOLD ANDROID WRAP SCRIPT
echo -----------------------------------------------------------------------

HERE="$(cd "$(dirname "$0")" && pwd)"

# Reference
# https://www.nrbtech.io/blog/2022/3/22/using-address-sanitizer-asan-to-debug-c-memory-issues-in-android-applications
# https://developer.android.com/ndk/guides/wrap-script
# https://github.com/android/ndk/issues/1297#issuecomment-651070101 (android:extractNativeLibs="true")

cmd=$1
shift

os_version=$(getprop ro.build.version.sdk)
#os_version=27

echo "MAWE OS VERSION: ${os_version}"

if [ "$os_version" -eq "27" ]; then
  cmd="$cmd -Xrunjdwp:transport=dt_android_adb,suspend=n,server=y -Xcompiler-option --debuggable $@"
elif [ "$os_version" -eq "28" ]; then
  cmd="$cmd -XjdwpProvider:adbconnection -XjdwpOptions:suspend=n,server=y -Xcompiler-option --debuggable $@"
else
  cmd="$cmd -XjdwpProvider:adbconnection -XjdwpOptions:suspend=n,server=y $@"
fi

ASAN_LIB=$(ls "$HERE"/libclang_rt.asan-*-android.so)
export LD_PRELOAD=${ASAN_LIB}
export ASAN_OPTIONS=log_to_syslog=false,allow_user_segv_handler=1,fast_unwind_on_malloc=1

exec $cmd

