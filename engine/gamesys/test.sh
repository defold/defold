#!/usr/bin/env bash

set -e

echo "**********************************************"
echo "CMAKE"
echo "**********************************************"

rm -rf ./build

echo "CMAKE CONFIGURATION"
time cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug -DTARGET_PLATFORM=arm64-macos -DBUILD_TESTS=OFF

# real    0m0.368s
# user    0m0.216s
# sys 0m0.183s

echo "NINJA BUILD"
time ninja -C build all install

# real    0m1.159s
# user    0m8.181s
# sys 0m7.314s

echo "**********************************************"
echo "Waf"
echo "**********************************************"

rm -rf ./build

echo "WAF CONFIGURATION"
time PREFIX=$DYNAMO_HOME waf configure --platform=arm64-macos

# real    0m0.270s
# user    0m0.420s
# sys 0m0.291s

echo "WAF BUILD"
time waf install --skip-build-tests


# real    0m2.488s
# user    0m12.699s
# sys 0m5.821s


rm -rf ./build

echo "**********************************************"
echo "DONE"
echo "**********************************************"

