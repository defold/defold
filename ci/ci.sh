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

# start X virtual frame buffer
if [[ "${OSTYPE}" == "linux-gnu" ]]; then
    echo "Starting xvfb"
    sudo apt-get install -y --no-install-recommends xvfb
    Xvfb :99 &
    export DISPLAY=:99
fi

echo "Calling ci.py with args: $@"
# -u to run python unbuffered to guarantee that output ends up in the correct order in logs
python -u ./ci/ci.py "$@"

# stop X virtual frame buffer
if [[ "${OSTYPE}" == "linux-gnu" ]]; then
    local xvfb_pids=`ps aux | grep tmp/xvfb-run | grep -v grep | awk '{print $2}'`
    if [ "$xvfb_pids" != "" ]; then
        echo "Killing the following xvfb processes: $xvfb_pids"
        sudo kill $xvfb_pids
    else
        echo "No xvfb processes to kill"
    fi
fi

echo -e "ci.sh finished\n-----------------\n\n\n\n\n"
