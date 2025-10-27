defold_log("sdk_xcode.cmake:")

# Disable requirement of having code signed executables when generating solutions
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

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
            list(SORT _XCODE_TOOLCHAINS COMPARE NATURAL)
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
    defold_log("sdk_xcode: Using Xcode toolchain: ${_XCODE_TOOLCHAIN}")
    # Set compilers only if not already defined to avoid mid-run changes
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${_XCODE_CXX}" CACHE FILEPATH "Xcode clang++" FORCE)
    endif()
    if(EXISTS "${_XCODE_CC}" AND NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER "${_XCODE_CC}" CACHE FILEPATH "Xcode clang" FORCE)
    endif()
    # ------------------------------------------------------------------
    # Discover SDK sysroots (populate variables for later selection)
    # - DEFOLD_SDK_MACOS_SYSROOT
    # - DEFOLD_SDK_IPHONEOS_SYSROOT
    # - DEFOLD_SDK_IPHONESIMULATOR_SYSROOT
    # ------------------------------------------------------------------

    # Reset cache entries (will be re-populated below if found)
    set(DEFOLD_SDK_MACOS_SYSROOT "" CACHE PATH "Path to macOS SDK sysroot" FORCE)
    set(DEFOLD_SDK_IPHONEOS_SYSROOT "" CACHE PATH "Path to iPhoneOS SDK sysroot" FORCE)
    set(DEFOLD_SDK_IPHONESIMULATOR_SYSROOT "" CACHE PATH "Path to iPhoneSimulator SDK sysroot" FORCE)

    # Packaged SDKs alongside toolchain
    if(DEFINED _SDKS_DIR AND _XCODE_TOOLCHAIN MATCHES "^${_SDKS_DIR}.*")
        # macOS SDKs
        file(GLOB _MACOS_SDKS "${_SDKS_DIR}/MacOSX*.sdk")
        if(_MACOS_SDKS)
            list(SORT _MACOS_SDKS COMPARE NATURAL)
            list(REVERSE _MACOS_SDKS)
            list(GET _MACOS_SDKS 0 _MACOS_SDK_PATH)
            if(EXISTS "${_MACOS_SDK_PATH}")
                set(DEFOLD_SDK_MACOS_SYSROOT "${_MACOS_SDK_PATH}" CACHE PATH "Path to macOS SDK sysroot" FORCE)
                defold_log("sdk_xcode: Found packaged macOS SDK: ${DEFOLD_SDK_MACOS_SYSROOT}")
            endif()
        endif()

        # iOS SDKs (device and simulator)
        file(GLOB _IOS_DEV_SDKS "${_SDKS_DIR}/iPhoneOS*.sdk")
        if(_IOS_DEV_SDKS)
            list(SORT _IOS_DEV_SDKS COMPARE NATURAL)
            list(REVERSE _IOS_DEV_SDKS)
            list(GET _IOS_DEV_SDKS 0 _IOS_DEV_SDK_PATH)
            if(EXISTS "${_IOS_DEV_SDK_PATH}")
                set(DEFOLD_SDK_IPHONEOS_SYSROOT "${_IOS_DEV_SDK_PATH}" CACHE PATH "Path to iPhoneOS SDK sysroot" FORCE)
                defold_log("sdk_xcode: Found packaged iPhoneOS SDK: ${DEFOLD_SDK_IPHONEOS_SYSROOT}")
            endif()
        endif()

        file(GLOB _IOS_SIM_SDKS "${_SDKS_DIR}/iPhoneSimulator*.sdk")
        if(_IOS_SIM_SDKS)
            list(SORT _IOS_SIM_SDKS COMPARE NATURAL)
            list(REVERSE _IOS_SIM_SDKS)
            list(GET _IOS_SIM_SDKS 0 _IOS_SIM_SDK_PATH)
            if(EXISTS "${_IOS_SIM_SDK_PATH}")
                set(DEFOLD_SDK_IPHONESIMULATOR_SYSROOT "${_IOS_SIM_SDK_PATH}" CACHE PATH "Path to iPhoneSimulator SDK sysroot" FORCE)
                defold_log("sdk_xcode: Found packaged iPhoneSimulator SDK: ${DEFOLD_SDK_IPHONESIMULATOR_SYSROOT}")
            endif()
        endif()
    endif()

    # Local Xcode SDKs
    if(APPLE)
        if(NOT _XCODE_DEV_DIR)
            execute_process(
                COMMAND xcode-select -print-path
                OUTPUT_VARIABLE _XCODE_DEV_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
        if(_XCODE_DEV_DIR)
            # macOS SDKs via Xcode.app
            set(_MACOS_SDK_DIR "${_XCODE_DEV_DIR}/Platforms/MacOSX.platform/Developer/SDKs")
            if(NOT DEFOLD_SDK_MACOS_SYSROOT AND EXISTS "${_MACOS_SDK_DIR}")
                file(GLOB _MACOS_SDKS_LOCAL "${_MACOS_SDK_DIR}/MacOSX*.sdk")
                if(_MACOS_SDKS_LOCAL)
                    list(SORT _MACOS_SDKS_LOCAL COMPARE NATURAL)
                    list(REVERSE _MACOS_SDKS_LOCAL)
                    list(GET _MACOS_SDKS_LOCAL 0 _MACOS_SDK_PATH_LOCAL)
                    if(EXISTS "${_MACOS_SDK_PATH_LOCAL}")
                        set(DEFOLD_SDK_MACOS_SYSROOT "${_MACOS_SDK_PATH_LOCAL}" CACHE PATH "Path to macOS SDK sysroot" FORCE)
                        defold_log("sdk_xcode: Found local macOS SDK: ${DEFOLD_SDK_MACOS_SYSROOT}")
                    endif()
                endif()
            endif()

            # iPhoneOS and iPhoneSimulator SDKs via Xcode.app
            set(_IOS_DEV_SDK_DIR "${_XCODE_DEV_DIR}/Platforms/iPhoneOS.platform/Developer/SDKs")
            if(NOT DEFOLD_SDK_IPHONEOS_SYSROOT AND EXISTS "${_IOS_DEV_SDK_DIR}")
                file(GLOB _IOS_DEV_SDKS_LOCAL "${_IOS_DEV_SDK_DIR}/iPhoneOS*.sdk")
                if(_IOS_DEV_SDKS_LOCAL)
                    list(SORT _IOS_DEV_SDKS_LOCAL COMPARE NATURAL)
                    list(REVERSE _IOS_DEV_SDKS_LOCAL)
                    list(GET _IOS_DEV_SDKS_LOCAL 0 _IOS_DEV_SDK_PATH_LOCAL)
                    if(EXISTS "${_IOS_DEV_SDK_PATH_LOCAL}")
                        set(DEFOLD_SDK_IPHONEOS_SYSROOT "${_IOS_DEV_SDK_PATH_LOCAL}" CACHE PATH "Path to iPhoneOS SDK sysroot" FORCE)
                        defold_log("sdk_xcode: Found local iPhoneOS SDK: ${DEFOLD_SDK_IPHONEOS_SYSROOT}")
                    endif()
                endif()
            endif()

            set(_IOS_SIM_SDK_DIR "${_XCODE_DEV_DIR}/Platforms/iPhoneSimulator.platform/Developer/SDKs")
            if(NOT DEFOLD_SDK_IPHONESIMULATOR_SYSROOT AND EXISTS "${_IOS_SIM_SDK_DIR}")
                file(GLOB _IOS_SIM_SDKS_LOCAL "${_IOS_SIM_SDK_DIR}/iPhoneSimulator*.sdk")
                if(_IOS_SIM_SDKS_LOCAL)
                    list(SORT _IOS_SIM_SDKS_LOCAL COMPARE NATURAL)
                    list(REVERSE _IOS_SIM_SDKS_LOCAL)
                    list(GET _IOS_SIM_SDKS_LOCAL 0 _IOS_SIM_SDK_PATH_LOCAL)
                    if(EXISTS "${_IOS_SIM_SDK_PATH_LOCAL}")
                        set(DEFOLD_SDK_IPHONESIMULATOR_SYSROOT "${_IOS_SIM_SDK_PATH_LOCAL}" CACHE PATH "Path to iPhoneSimulator SDK sysroot" FORCE)
                        defold_log("sdk_xcode: Found local iPhoneSimulator SDK: ${DEFOLD_SDK_IPHONESIMULATOR_SYSROOT}")
                    endif()
                endif()
            endif()
        endif()

        # Command Line Tools macOS SDK fallback
        if(NOT DEFOLD_SDK_MACOS_SYSROOT AND EXISTS "/Library/Developer/CommandLineTools/SDKs")
            file(GLOB _CLT_MACOS_SDKS "/Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk")
            if(_CLT_MACOS_SDKS)
                list(SORT _CLT_MACOS_SDKS COMPARE NATURAL)
                list(REVERSE _CLT_MACOS_SDKS)
                list(GET _CLT_MACOS_SDKS 0 _CLT_MACOS_SDK_PATH)
                if(EXISTS "${_CLT_MACOS_SDK_PATH}")
                    set(DEFOLD_SDK_MACOS_SYSROOT "${_CLT_MACOS_SDK_PATH}" CACHE PATH "Path to macOS SDK sysroot" FORCE)
                    defold_log("sdk_xcode: Found CLT macOS SDK: ${DEFOLD_SDK_MACOS_SYSROOT}")
                endif()
            endif()
        endif()
    endif()

    # ------------------------------------------------------------------
    # Prefer an SDK sysroot matching TARGET_PLATFORM (uses discovered vars)
    # ------------------------------------------------------------------
    if(TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        if(TARGET_PLATFORM MATCHES "^x86_64-ios$")
            if(DEFOLD_SDK_IPHONESIMULATOR_SYSROOT)
                set(CMAKE_OSX_SYSROOT "${DEFOLD_SDK_IPHONESIMULATOR_SYSROOT}" CACHE PATH "Selected iOS Simulator SDK sysroot" FORCE)
                defold_log("sdk_xcode: Using iOS Simulator SDK: ${CMAKE_OSX_SYSROOT}")
            endif()
        else()
            if(DEFOLD_SDK_IPHONEOS_SYSROOT)
                set(CMAKE_OSX_SYSROOT "${DEFOLD_SDK_IPHONEOS_SYSROOT}" CACHE PATH "Selected iPhoneOS SDK sysroot" FORCE)
                defold_log("sdk_xcode: Using iPhoneOS SDK: ${CMAKE_OSX_SYSROOT}")
            endif()
        endif()
    elseif(TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        if(DEFOLD_SDK_MACOS_SYSROOT)
            set(CMAKE_OSX_SYSROOT "${DEFOLD_SDK_MACOS_SYSROOT}" CACHE PATH "Selected macOS SDK sysroot" FORCE)
            defold_log("sdk_xcode: Using macOS SDK: ${CMAKE_OSX_SYSROOT}")
        endif()
    endif()
else()
    message(DEBUG "sdk_xcode: No suitable Xcode clang++ found; leaving default compilers")
endif()
