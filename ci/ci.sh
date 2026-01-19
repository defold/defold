#!/bin/bash
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.


set -e

OLDPATH=$PATH

export DEFOLD_EDITOR_DISABLE_PERFORMANCE_TESTS=1

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
echo "PYTHON=" $(which python)

echo "Calling ci.py with args: $@"
# # -u to run python unbuffered to guarantee that output ends up in the correct order in logs
if [[ "${OSTYPE}" == "linux-gnu" ]]; then
    xvfb-run --auto-servernum python -u ./ci/ci.py "$@"
else
    python -u ./ci/ci.py "$@"
fi

echo -e "ci.sh finished\n-----------------\n\n\n\n\n"
