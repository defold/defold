#!/usr/bin/env python
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
from private_hooks import find_hook_attr


def main():
    if len(sys.argv) < 2:
        print("Usage: set_sdk_vars.py VAR1 VAR2 ...")
        sys.exit(1)

    for var_name in sys.argv[1:]:
        attr = getattr(sdk, var_name, None)
        if attr is None:
            attr = find_hook_attr('sdk', var_name)

        if attr is None:
            print(f"Error: {var_name} is not defined in sdk.py", file=sys.stderr)
            sys.exit(1)

        print(f"{var_name}={attr}")

if __name__ == "__main__":
    main()
