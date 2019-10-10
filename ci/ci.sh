#!/bin/bash
./scripts/build.py --save-env-path=ci/env.sh save_env
source ci/env.sh

echo "DYNAMO $DYNAMO_HOME"
echo "PATH $PATH"
./scripts/build.py --platform=x86_64-darwin install_ext
./scripts/build.py --platform=x86_64-darwin --skip-tests build_engine -- --skip-build-tests
#./scripts/build.py --platform=x86_64-darwin build_engine
PREFIX=$DYNAMO_HOME

cd engine/sound
# waf configure --platform=x86_64-darwin
echo "FOOBAR BEFORE build skip tests"
waf build --skip-tests
echo "FOOBAR AFTER build skip tests"


echo "FOOBAR BEFORE build and test"
waf build
echo "FOOBAR AFTER build and test"

cd ../engine/liveupdate
# waf configure --platform=x86_64-darwin
waf build

# python ./ci/ci.py "$@"
