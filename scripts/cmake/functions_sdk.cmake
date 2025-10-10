if(DEFINED DEFOLD_FUNCTIONS_SDK_CMAKE_INCLUDED)
    return()
endif()
set(DEFOLD_FUNCTIONS_SDK_CMAKE_INCLUDED ON)

defold_log("functions_sdk.cmake:")

# Read a constant from build_tools/sdk.py and set a CMake variable.
# Usage:
#   defold_set_from_sdk_py(SOURCE <python_const> TARGET <cmake_var>)
# Example:
#   defold_set_from_sdk_py(SOURCE VERSION_IPHONEOS_MIN TARGET SDK_VERSION_IPHONEOS_MIN)
function(defold_set_from_sdk_py)
    set(options)
    set(oneValueArgs SOURCE TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(DSDK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DSDK_SOURCE OR NOT DSDK_TARGET)
        message(FATAL_ERROR "defold_set_from_sdk_py: requires SOURCE <python_const> and TARGET <cmake_var>")
    endif()

    # Resolve repo root to locate build_tools/sdk.py
    set(_DEFOLD_HOME "")
    if(DEFINED DEFOLD_HOME)
        set(_DEFOLD_HOME "${DEFOLD_HOME}")
    else()
        get_filename_component(_moddir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
        get_filename_component(_DEFOLD_HOME "${_moddir}/../.." ABSOLUTE)
    endif()

    set(_SDK_PY "${_DEFOLD_HOME}/build_tools/sdk.py")
    if(NOT EXISTS "${_SDK_PY}")
        message(FATAL_ERROR "defold_set_from_sdk_py: Not found: ${_SDK_PY}")
    endif()

    # Extract the assignment line for the requested constant.
    # Handles values with or without quotes, ignores any trailing comments.
    file(STRINGS "${_SDK_PY}" _line REGEX "^[ \t]*${DSDK_SOURCE}[ \t]*=")
    if(NOT _line)
        message(FATAL_ERROR "defold_set_from_sdk_py: Constant not found in sdk.py: ${DSDK_SOURCE}")
    endif()

    # Remove prefix '<CONST> = ' and optional quotes; strip inline comments.
    # Example matches:
    #   VERSION_IPHONEOS_MIN="11.0"    -> 11.0
    #   ANDROID_TARGET_API_LEVEL = 35  -> 35
    string(REGEX REPLACE "^[ \t]*${DSDK_SOURCE}[ \t]*=[ \t]*['\"]?([^'\"#]+).*" "\\1" _value "${_line}")
    string(STRIP "${_value}" _value)

    # Export to caller's scope
    set(${DSDK_TARGET} "${_value}" PARENT_SCOPE)
endfunction()
