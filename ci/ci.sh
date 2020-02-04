#!/bin/bash
set -e

OLDPATH=$PATH

echo -e "\n\n\n\n\n-----------------\nci.sh started with args: $@"
echo "Saving environment variables to file"
./scripts/build.py --save-env-path=ci/env.sh save_env

echo "Saved environment variables:"
cat ci/env.sh

echo "Exporting saved environment variables"
source ci/env.sh

# on windows, we get windows formatted paths, which won't work in mingw
# so we've stored the old paths, and append them.
export PATH=$PATH:$OLDPATH
echo "PATH=" $PATH

echo "Calling ci.py with args: $@"
# # -u to run python unbuffered to guarantee that output ends up in the correct order in logs
if [[ "${OSTYPE}" == "linux-gnu" ]]; then
    xvfb-run --auto-servernum python -u ./ci/ci.py "$@"
else
    python -u ./ci/ci.py "$@"
fi

echo -e "ci.sh finished\n-----------------\n\n\n\n\n"
