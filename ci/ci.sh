#!/bin/bash
echo -e "\n\n\n\n\n-----------------ci.sh was started with args: $@"
echo "Saving environment variables to file"
./scripts/build.py --save-env-path=ci/env.sh save_env

echo "Sourcing saved env"
cat ci/env.sh
source ci/env.sh

echo "GITHUB_REF is ${GITHUB_REF}"
echo "Calling ci.py with args: $@"
python ./ci/ci.py "$@"

echo -e "Done calling cp.py with args: $@\n-----------------"
