#!/system/bin/sh
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.


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

