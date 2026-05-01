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

# The existance of this

function cmi_setup_cc_private() {
    echo "Checking for supported private platforms"
}

function cmi_private() {
    export PREFIX=`pwd`/build
    export PLATFORM=$1

    case $PLATFORM in
        arm64-nx64)
            echo "Has arm64-nx64 support"
            cmi_cross $PLATFORM $PLATFORM
            ;;

        *)
            echo "Unknown target $1" && exit 1
            ;;
    esac
}