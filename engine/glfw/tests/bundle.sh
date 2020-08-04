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



# Creates application bundles for use on Mac OS X.

if [ -z "$1" ]; then
  echo "usage: `basename $0` BUNDLE-NAME"
  exit 1
fi

bundle_name="$1"

if [ ! -d "${bundle_name}.app/Contents/MacOS" ]; then
  mkdir -p "${bundle_name}.app/Contents/MacOS"
fi

if [ ! -d "${bundle_name}.app/Contents/Resources" ]; then
  mkdir -p "${bundle_name}.app/Contents/Resources"
fi

if [ ! -f "${bundle_name}.app/Contents/PkgInfo" ]; then
  echo -n "APPL????" > "${bundle_name}.app/Contents/PkgInfo"
fi

if [ ! -f "${bundle_name}.app/Contents/Info.plist" ]; then
  cat > "${bundle_name}.app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>CFBundleDevelopmentRegion</key>
        <string>English</string>
        <key>CFBundleExecutable</key>
        <string>${bundle_name}</string>
        <key>CFBundleInfoDictionaryVersion</key>
        <string>6.0</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>CFBundleSignature</key>
        <string>????</string>
        <key>CFBundleVersion</key>
        <string>0.1</string>
</dict>
</plist>
EOF
fi

