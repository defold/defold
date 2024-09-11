# Copyright 2020-2024 The Defold Foundation
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
platform=`uname -s`

if [ "Darwin" == $platform ]; then
    # NOTE: We used to link with -flat_namespace but go spurious link errors
    export DYLD_FORCE_FLAT_NAMESPACE=yes
    export DYLD_INSERT_LIBRARIES=${DYNAMO_HOME}/lib/arm64-macos/libdlib_memprofile.dylib
fi

if [ "Linux" == $platform ]; then
    export LD_PRELOAD=${DYNAMO_HOME}/lib/x86_64-linux/libdlib_memprofile.so
fi

PROGRAM=$(which dmengine)
if [ "$1" != "" ]; then
    PROGRAM=$1
fi

# memprofile.trace is written in the current working dir
# Also see engine/dlib/README.md for instruction on how to generate a report
DMMEMPROFILE_TRACE=1 ${PROGRAM} $@
