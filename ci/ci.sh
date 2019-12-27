#!/bin/bash
set -e

echo -e "\n\n\n\n\n-----------------\nci.sh started with args: $@"
echo "Saving environment variables to file"
./scripts/build.py --save-env-path=ci/env.sh save_env

echo "Saved environment variables:"
cat ci/env.sh

echo "Exporting saved environment variables"
source ci/env.sh

echo "Calling ci.py with args: $@"
# -u to run python unbuffered to guarantee that output ends up in the correct order in logs
python -u ./ci/ci.py "$@"

echo -e "ci.sh finished\n-----------------\n\n\n\n\n"
