#!/usr/bin/env bash

set -e

# adb push /Users/mathiaswesterdahl/work/defold/engine/hid/build/src/test/test_app_hid /data/local/tmp/unittest/hid/test_app_hid
# adb shell chmod 755 /data/local/tmp/unittest/hid/test_app_hid
# adb shell "/data/local/tmp/unittest/hid/test_app_hid"

APK=./build/src/test/test_app_hid.android/test_app_hid.apk
adb install -r "$APK"

adb logcat -c
adb shell am start -n com.defold.test_app_hid/com.dynamo.android.DefoldActivity
PID=$(adb shell pidof -s com.defold.test_app_hid | tr -d '\r')
adb logcat --pid="$PID"
