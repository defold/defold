message("sdk_xcode.cmake:")

# Detect packaged or local Xcode toolchain and set compilers (clang/clang++)

set(_XCODE_TOOLCHAIN "")
set(_XCODE_CXX "")
set(_XCODE_CC  "")

# 1) Look for packaged Xcode inside tmp/dynamo_home/ext/SDKs
if(DEFINED DEFOLD_SDK_ROOT)
    set(_SDKS_DIR "${DEFOLD_SDK_ROOT}/ext/SDKs")
    if(EXISTS "${_SDKS_DIR}")
        file(GLOB _XCODE_TOOLCHAINS "${_SDKS_DIR}/XcodeDefault*.xctoolchain")
        if(_XCODE_TOOLCHAINS)
            list(SORT _XCODE_TOOLCHAINS)
            list(REVERSE _XCODE_TOOLCHAINS)
            foreach(_tc ${_XCODE_TOOLCHAINS})
                set(_cxx "${_tc}/usr/bin/clang++")
                set(_cc  "${_tc}/usr/bin/clang")
                if(EXISTS "${_cxx}")
                    set(_XCODE_TOOLCHAIN "${_tc}")
                    set(_XCODE_CXX "${_cxx}")
                    if(EXISTS "${_cc}")
                        set(_XCODE_CC "${_cc}")
                    endif()
                    break()
                endif()
            endforeach()
        else()
            message(DEBUG "sdk_xcode: No packaged Xcode toolchain found in ${_SDKS_DIR}")
        endif()
    else()
        message(DEBUG "sdk_xcode: SDKs directory not found: ${_SDKS_DIR}")
    endif()
else()
    message(DEBUG "sdk_xcode: DEFOLD_SDK_ROOT not set; skipping packaged Xcode detection")
endif()

# 2) If none packaged, try to find locally installed Xcode toolchain on macOS
if(NOT _XCODE_TOOLCHAIN AND APPLE)
    # xcode-select -print-path -> /Applications/Xcode.app/Contents/Developer
    execute_process(
        COMMAND xcode-select -print-path
        OUTPUT_VARIABLE _XCODE_DEV_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_XCODE_DEV_DIR AND EXISTS "${_XCODE_DEV_DIR}/Toolchains/XcodeDefault.xctoolchain")
        set(_XCODE_TOOLCHAIN "${_XCODE_DEV_DIR}/Toolchains/XcodeDefault.xctoolchain")
        set(_XCODE_CXX "${_XCODE_TOOLCHAIN}/usr/bin/clang++")
        set(_XCODE_CC  "${_XCODE_TOOLCHAIN}/usr/bin/clang")
    elseif(EXISTS "/Library/Developer/CommandLineTools/usr/bin/clang++")
        # Fallback to Command Line Tools
        set(_XCODE_TOOLCHAIN "/Library/Developer/CommandLineTools")
        set(_XCODE_CXX "/Library/Developer/CommandLineTools/usr/bin/clang++")
        set(_XCODE_CC  "/Library/Developer/CommandLineTools/usr/bin/clang")
    endif()
endif()

if(_XCODE_TOOLCHAIN AND EXISTS "${_XCODE_CXX}")
    message(STATUS "sdk_xcode: Using Xcode toolchain: ${_XCODE_TOOLCHAIN}")
    # Set C++ compiler to detected clang++ (cache, force to override defaults)
    set(CMAKE_CXX_COMPILER "${_XCODE_CXX}" CACHE FILEPATH "Xcode clang++" FORCE)
    if(EXISTS "${_XCODE_CC}")
        set(CMAKE_C_COMPILER "${_XCODE_CC}" CACHE FILEPATH "Xcode clang" FORCE)
    endif()
else()
    message(DEBUG "sdk_xcode: No suitable Xcode clang++ found; leaving default compilers")
endif()
