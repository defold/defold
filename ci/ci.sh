#!/bin/bash
echo -e "\n\n\n\n\n-----------------\nci.sh started with args: $@"
echo "Saving environment variables to file"
./scripts/build.py --save-env-path=ci/env.sh save_env

echo "Sourcing saved env"
cat ci/env.sh
source ci/env.sh

echo "GITHUB_REF is ${GITHUB_REF}"
echo "Calling ci.py with args: $@"
python -u ./ci/ci.py "$@"

echo -e "ci.sh finished\n-----------------\n\n\n\n\n"
