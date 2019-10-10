#!/bin/bash
./scripts/build.py --save-env-path=ci/env.sh save_env
source ci/env.sh

echo "DYNAMO $DYNAMO_HOME"
echo "PATH $PATH"
./scripts/build.py --platform=x86_64-darwin install_ext
# ./scripts/build.py --platform=x86_64-darwin --skip-tests build_engine -- --skip-build-tests
./scripts/build.py --platform=x86_64-darwin build_engine
# cd engine/liveupdate
# PREFIX=$DYNAMO_HOME
# waf configure --platform=x86_64-darwin

# python ./ci/ci.py "$@"
