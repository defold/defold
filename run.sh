#!/usr/bin/env bash

PLATFORM=$1
shift

#PLATFORM=arm64-macos
#PLATFORM=wasm-web
#PLATFORM=js-web
#PLATFORM=wasm_pthread-web

if [ "" != "$PLATFORM" ]; then
    PLATFORM_ARG="--platform=${PLATFORM}"
fi

echo "Using PLATFORM=${PLATFORM}"

set -e

rm -rf ./tmp/dynamo_home/ext/lib/${PLATFORM}
rm -rf ./tmp/dynamo_home/lib/${PLATFORM}

./scripts/build.py distclean

./scripts/build.py install_ext ${PLATFORM_ARG}

./scripts/build.py build_engine ${PLATFORM_ARG} --skip-docs --skip-tests -- --skip-build-tests --opt-level=0

./scripts/build.py build_bob --skip-tests

# (cd editor && lein init)

say "Compilation Finished"

# echo "******************************************************"
# echo "Done!"

# ls -la engine/engine/build/src/dmengine
# strip engine/engine/build/src/dmengine
# ls -la engine/engine/build/src/dmengine


# echo "******************************************************"

