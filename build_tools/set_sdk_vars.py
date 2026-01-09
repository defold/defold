#!/usr/bin/env python
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



"""
This is a helper script to set up variables from `sdk.py` in shell scripts.
Usage:
eval $(python $DEFOLD_HOME/build_tools/set_sdk_vars.py NEEDED_VARS_FROM_SDK_PY)

For example:

eval $(python $DEFOLD_HOME/build_tools/set_sdk_vars.py ANDROID_NDK_VERSION ANDROID_BUILD_TOOLS_VERSION)
echo $ANDROID_NDK_VERSION
echo $ANDROID_BUILD_TOOLS_VERSION
"""

import sys
import sdk

def main():
    if len(sys.argv) < 2:
        print("Usage: set_sdk_vars.py VAR1 VAR2 ...")
        sys.exit(1)

    for var_name in sys.argv[1:]:
        if hasattr(sdk, var_name):
            print(f"{var_name}={getattr(sdk, var_name)}")
        else:
            print(f"Error: {var_name} is not defined in sdk.py", file=sys.stderr)
            sys.exit(1)

if __name__ == "__main__":
    main()
