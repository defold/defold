#!/bin/bash
./scripts/build.py --save-env-path=ci/env.sh save_env
source ci/env.sh

echo "DYNAMO $DYNAMO_HOME"
echo "PATH $PATH"
cd engine/liveupdate
PREFIX=$DYNAMO_HOME waf configure --platform=x86_64-darwin

# python ./ci/ci.py "$@"
