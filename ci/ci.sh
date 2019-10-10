#!/bin/bash
./scripts/build.py --save-env-path=ci/env.sh save_env
source ci/env.sh
python ./ci/ci.py "$@"

# echo "DYNAMO $DYNAMO_HOME"
# echo "PATH $PATH"
# ./scripts/build.py --platform=x86_64-darwin install_ext
# ./scripts/build.py --platform=x86_64-darwin --skip-tests build_engine -- --skip-build-tests
# PREFIX=$DYNAMO_HOME
# cd engine/sound
# waf build
#
