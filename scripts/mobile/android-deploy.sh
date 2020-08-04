#!/bin/sh
# Copyright 2020 The Defold Foundation
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



PACKAGE=$1

if [ -z "$PACKAGE" ]; then
    echo "You must pass a .apk as input!"
    exit 1
fi

ANDROID_HOME=$DYNAMO_HOME/ext/SDKs/android-sdk
AAPT=$(find $ANDROID_HOME -iname "aapt")

PACKAGENAME=`$AAPT dump badging "$PACKAGE" | grep package:\ name | cut -d \' -f 2`

echo "Package name = $PACKAGENAME"

echo "Uninstalling $PACKAGENAME"

adb uninstall $PACKAGENAME | sed -e 's/^/    /'

sleep 1

echo "Installing $PACKAGENAME"

adb install "$PACKAGE" | sed -e 's/^/    /'

echo "Launching $PACKAGENAME"

adb shell monkey -p $PACKAGENAME -c android.intent.category.LAUNCHER 1

sleep 1

PID=`adb shell ps | grep $PACKAGENAME | awk '{ print $2 }'`

echo PID=$PID

adb logcat | grep $PID


