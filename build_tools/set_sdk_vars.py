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

try:
    import sdk_vendor
except ModuleNotFoundError as e:
    # Currently, the output is parsed by other scripts
    if "No module named 'sdk_vendor'" in str(e):
        pass
    else:
        raise e
except Exception as e:
    print("Failed to import sdk_vendor.py:")
    raise e

if 'sdk_vendor' not in sys.modules:
    sdk_vendor = None


def main():
    if len(sys.argv) < 2:
        print("Usage: set_sdk_vars.py VAR1 VAR2 ...")
        sys.exit(1)

    for var_name in sys.argv[1:]:
        attr = getattr(sdk, var_name, None)
        if attr is None:
            attr = getattr(sdk_vendor, var_name, None)

        if attr is None:
            print(f"Error: {var_name} is not defined in sdk.py", file=sys.stderr)
            sys.exit(1)

        print(f"{var_name}={attr}")

if __name__ == "__main__":
    main()
