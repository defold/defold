message("sdk_windows.cmake:")

# Detect Windows toolchain and SDK from:
# 1) Packaged SDKs under ${DEFOLD_SDK_ROOT}/ext/SDKs (MSVC preferred over LLVM)
# 2) Locally installed Visual Studio (preferred over local LLVM/Clang)
# 3) Packaged LLVM/Clang under ${DEFOLD_SDK_ROOT}/ext/SDKs
# 4) Locally installed LLVM/Clang (clang/clang++)
#
# Results:
# - Sets CMAKE_{C,CXX}_COMPILER when not already provided
# - Tries to detect a Windows SDK version and sets CMAKE_SYSTEM_VERSION

if(NOT WIN32)
    message(DEBUG "sdk_windows: Non-Windows host; nothing to do")
    return()
endif()

set(_DEFOLD_WIN_ARCH "x64")
if(DEFINED TARGET_PLATFORM)
    if(TARGET_PLATFORM MATCHES "(^|-)x86($|-)" OR TARGET_PLATFORM MATCHES "x86-win32")
        set(_DEFOLD_WIN_ARCH "x86")
    endif()
endif()

set(_FOUND_MSVC_CL "")
set(_FOUND_CLANG_CL "")
set(_FOUND_LLVM_CLANGXX "")
set(_FOUND_LLVM_CLANG   "")
set(_FOUND_WINSDK_VERSION "")

# Helper to pick latest directory path from a list of paths
function(_defold_pick_latest_dir OUT_VAR)
    set(dirs ${ARGN})
    if(dirs)
        list(SORT dirs)
        list(REVERSE dirs)
        list(GET dirs 0 _latest)
        set(${OUT_VAR} "${_latest}" PARENT_SCOPE)
    endif()
endfunction()

# Helper: detect Windows SDK version under a root (Windows Kits/10/Include/<ver>)
function(_defold_detect_winsdk_from_root ROOT OUT_VER)
    set(${OUT_VER} "" PARENT_SCOPE)
    if(NOT EXISTS "${ROOT}")
        return()
    endif()
    set(_inc_root "${ROOT}/Windows Kits/10/Include")
    if(EXISTS "${_inc_root}")
        file(GLOB _sdk_vers RELATIVE "${_inc_root}" "${_inc_root}/*")
        if(_sdk_vers)
            _defold_pick_latest_dir(_latest_ver ${_sdk_vers})
            if(_latest_ver)
                set(${OUT_VER} "${_latest_ver}" PARENT_SCOPE)
            endif()
        endif()
    endif()
endfunction()

# Helper: detect cl.exe under a Visual Studio root
function(_defold_detect_msvc_cl_from_vsroot VSROOT ARCH OUT_PATH)
    set(${OUT_PATH} "" PARENT_SCOPE)
    if(NOT EXISTS "${VSROOT}")
        return()
    endif()
    if(ARCH STREQUAL "x64")
        set(_cl_globs "${VSROOT}/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")
    else()
        set(_cl_globs "${VSROOT}/VC/Tools/MSVC/*/bin/Hostx86/x86/cl.exe")
    endif()
    foreach(_pat IN LISTS _cl_globs)
        file(GLOB _matches ${_pat})
        if(_matches)
            _defold_pick_latest_dir(_best ${_matches})
            if(_best)
                set(${OUT_PATH} "${_best}" PARENT_SCOPE)
                return()
            endif()
        endif()
    endforeach()
endfunction()

# Helper: detect clang-cl.exe under a Visual Studio root
function(_defold_detect_clang_cl_from_vsroot VSROOT ARCH OUT_PATH)
    set(${OUT_PATH} "" PARENT_SCOPE)
    if(NOT EXISTS "${VSROOT}")
        return()
    endif()
    if(ARCH STREQUAL "x64")
        set(_globs "${VSROOT}/VC/Tools/Llvm/x64/bin/clang-cl.exe")
    else()
        set(_globs "${VSROOT}/VC/Tools/Llvm/x86/bin/clang-cl.exe")
    endif()
    file(GLOB _matches ${_globs})
    if(_matches)
        _defold_pick_latest_dir(_best ${_matches})
        if(_best)
            set(${OUT_PATH} "${_best}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# 1) Packaged SDKs in DEFOLD_SDK_ROOT/ext/SDKs (prefer MSVC over LLVM)
if(DEFINED DEFOLD_SDK_ROOT)
    set(_SDKS_DIR "${DEFOLD_SDK_ROOT}/ext/SDKs")
    if(EXISTS "${_SDKS_DIR}")
        # Packaged Visual Studio layout (if any)
        file(GLOB _vs_roots
            "${_SDKS_DIR}/Microsoft Visual Studio/*/*"
            "${_SDKS_DIR}/VS/*"
            "${_SDKS_DIR}/BuildTools/*"
        )
        foreach(_vs IN LISTS _vs_roots)
            if(NOT _FOUND_MSVC_CL)
                _defold_detect_msvc_cl_from_vsroot("${_vs}" "${_DEFOLD_WIN_ARCH}" _cl)
                if(_cl)
                    set(_FOUND_MSVC_CL "${_cl}")
                endif()
            endif()
        endforeach()

        # Packaged Windows SDK version
        if(NOT _FOUND_WINSDK_VERSION)
            _defold_detect_winsdk_from_root("${_SDKS_DIR}" _ver)
            if(_ver)
                set(_FOUND_WINSDK_VERSION "${_ver}")
            endif()
        endif()

        # Packaged LLVM/Clang (prefer clang-cl if available)
        if(NOT _FOUND_CLANG_CL)
            file(GLOB _clangcl_cands
                "${_SDKS_DIR}/LLVM*/bin/clang-cl.exe"
                "${_SDKS_DIR}/clang*/bin/clang-cl.exe"
                "${_SDKS_DIR}/llvm*/bin/clang-cl.exe"
            )
            if(_clangcl_cands)
                _defold_pick_latest_dir(_clangcl "${_clangcl_cands}")
                if(_clangcl)
                    set(_FOUND_CLANG_CL "${_clangcl}")
                endif()
            endif()
        endif()

        if(NOT _FOUND_CLANG_CL AND NOT _FOUND_MSVC_CL AND NOT _FOUND_LLVM_CLANGXX)
            file(GLOB _clangxx_cands
                "${_SDKS_DIR}/LLVM*/bin/clang++.exe"
                "${_SDKS_DIR}/clang*/bin/clang++.exe"
                "${_SDKS_DIR}/llvm*/bin/clang++.exe"
            )
            if(_clangxx_cands)
                _defold_pick_latest_dir(_clangxx "${_clangxx_cands}")
                if(_clangxx)
                    set(_FOUND_LLVM_CLANGXX "${_clangxx}")
                    get_filename_component(_clang_dir "${_clangxx}" DIRECTORY)
                    if(EXISTS "${_clang_dir}/clang.exe")
                        set(_FOUND_LLVM_CLANG "${_clang_dir}/clang.exe")
                    endif()
                endif()
            endif()
        endif()
    else()
        message(DEBUG "sdk_windows: SDKs directory not found: ${_SDKS_DIR}")
    endif()
else()
    message(DEBUG "sdk_windows: DEFOLD_SDK_ROOT not set; skipping packaged SDK detection")
endif()

# 2) Locally installed Visual Studio (prefer over local LLVM)
if(NOT _FOUND_MSVC_CL)
    # Use vswhere if available
    find_program(_VSWHERE vswhere HINTS
        "C:/Program Files (x86)/Microsoft Visual Studio/Installer"
    )
    if(_VSWHERE)
        execute_process(
            COMMAND "${_VSWHERE}" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            OUTPUT_VARIABLE _VS_INSTALL
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_VS_INSTALL AND EXISTS "${_VS_INSTALL}")
            _defold_detect_msvc_cl_from_vsroot("${_VS_INSTALL}" "${_DEFOLD_WIN_ARCH}" _cl)
            if(_cl)
                set(_FOUND_MSVC_CL "${_cl}")
            endif()
            if(NOT _FOUND_CLANG_CL)
                _defold_detect_clang_cl_from_vsroot("${_VS_INSTALL}" "${_DEFOLD_WIN_ARCH}" _clangcl)
                if(_clangcl)
                    set(_FOUND_CLANG_CL "${_clangcl}")
                endif()
            endif()
        endif()
    endif()

    if(NOT _FOUND_MSVC_CL)
        # Common fallback locations
        set(_VS_FALLBACKS
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community"
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools"
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
        )
        foreach(_root IN LISTS _VS_FALLBACKS)
            if((NOT _FOUND_MSVC_CL OR NOT _FOUND_CLANG_CL) AND EXISTS "${_root}")
                _defold_detect_msvc_cl_from_vsroot("${_root}" "${_DEFOLD_WIN_ARCH}" _cl)
                if(_cl)
                    set(_FOUND_MSVC_CL "${_cl}")
                endif()
                if(NOT _FOUND_CLANG_CL)
                    _defold_detect_clang_cl_from_vsroot("${_root}" "${_DEFOLD_WIN_ARCH}" _clangcl)
                    if(_clangcl)
                        set(_FOUND_CLANG_CL "${_clangcl}")
                    endif()
                endif()
            endif()
        endforeach()
    endif()

    # Windows SDK version from system install
    if(NOT _FOUND_WINSDK_VERSION)
        _defold_detect_winsdk_from_root("C:/Program Files (x86)" _ver)
        if(_ver)
            set(_FOUND_WINSDK_VERSION "${_ver}")
        endif()
    endif()
endif()

# 3) Packaged LLVM/Clang if no MSVC yet (already checked above under packaged section)
# 4) Local LLVM/Clang (clang++) if nothing else found (or if user overrides)
if(NOT _FOUND_CLANG_CL AND NOT _FOUND_MSVC_CL AND NOT _FOUND_LLVM_CLANGXX)
    # Prefer locally installed Visual Studio over LLVM/Clang (handled above)
    # Now look for a local LLVM install (prefer clang-cl)
    find_program(_CLANGCL clang-cl HINTS
        "C:/Program Files/LLVM/bin"
        "C:/Program Files (x86)/LLVM/bin"
    )
    if(_CLANGCL)
        set(_FOUND_CLANG_CL "${_CLANGCL}")
    else()
        find_program(_CLANGXX clang++ HINTS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
        )
        if(_CLANGXX)
            set(_FOUND_LLVM_CLANGXX "${_CLANGXX}")
            get_filename_component(_clang_dir "${_CLANGXX}" DIRECTORY)
            find_program(_CLANG clang HINTS "${_clang_dir}")
            if(_CLANG)
                set(_FOUND_LLVM_CLANG "${_CLANG}")
            endif()
        endif()
    endif()

    # Try Git for Windows LLVM bundle (sometimes installed via VSCode toolchains)
    if(NOT _FOUND_LLVM_CLANGXX)
        find_program(_CLANGCL2 clang-cl)
        if(_CLANGCL2 AND NOT _FOUND_CLANG_CL)
            set(_FOUND_CLANG_CL "${_CLANGCL2}")
        endif()
        if(NOT _FOUND_CLANG_CL)
            find_program(_CLANGXX2 clang++)
            if(_CLANGXX2)
                set(_FOUND_LLVM_CLANGXX "${_CLANGXX2}")
            endif()
            find_program(_CLANG2 clang)
            if(NOT _FOUND_LLVM_CLANG AND _CLANG2)
                set(_FOUND_LLVM_CLANG "${_CLANG2}")
            endif()
        endif()
    endif()

    # Windows SDK version from system if not set
    if(NOT _FOUND_WINSDK_VERSION)
        _defold_detect_winsdk_from_root("C:/Program Files (x86)" _ver2)
        if(_ver2)
            set(_FOUND_WINSDK_VERSION "${_ver2}")
        endif()
    endif()
endif()

# Finalize: choose compilers respecting preference:
# - If MSVC found, use cl.exe
# - Else, use clang/clang++ if found

if(_FOUND_CLANG_CL)
    message(STATUS "sdk_windows: Using clang-cl: ${_FOUND_CLANG_CL}")
    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER "${_FOUND_CLANG_CL}" CACHE FILEPATH "LLVM C compiler (clang-cl)" FORCE)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${_FOUND_CLANG_CL}" CACHE FILEPATH "LLVM C++ compiler (clang-cl)" FORCE)
    endif()
elseif(_FOUND_MSVC_CL)
    message(STATUS "sdk_windows: Using MSVC cl: ${_FOUND_MSVC_CL}")
    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER "${_FOUND_MSVC_CL}" CACHE FILEPATH "MSVC C compiler" FORCE)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${_FOUND_MSVC_CL}" CACHE FILEPATH "MSVC C++ compiler" FORCE)
    endif()
elseif(_FOUND_LLVM_CLANGXX)
    message(STATUS "sdk_windows: Using LLVM toolchain: ${_FOUND_LLVM_CLANGXX}")
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${_FOUND_LLVM_CLANGXX}" CACHE FILEPATH "LLVM C++ compiler" FORCE)
    endif()
    if(NOT CMAKE_C_COMPILER)
        if(_FOUND_LLVM_CLANGXX MATCHES "clang-cl\.exe$")
            set(CMAKE_C_COMPILER "${_FOUND_LLVM_CLANGXX}" CACHE FILEPATH "LLVM C compiler (clang-cl)" FORCE)
        elseif(_FOUND_LLVM_CLANG)
            set(CMAKE_C_COMPILER "${_FOUND_LLVM_CLANG}" CACHE FILEPATH "LLVM clang" FORCE)
        else()
            find_program(_FALLBACK_CLANG clang)
            if(_FALLBACK_CLANG)
                set(CMAKE_C_COMPILER "${_FALLBACK_CLANG}" CACHE FILEPATH "LLVM clang" FORCE)
            endif()
        endif()
    endif()
else()
    message(WARNING "sdk_windows: No suitable MSVC or LLVM toolchain found. CMake default compilers will be used.")
endif()

# Set Windows SDK version if discovered (helps CMake locate SDK libs/headers)
if(_FOUND_WINSDK_VERSION)
    # CMake uses CMAKE_SYSTEM_VERSION to control chosen Windows SDK
    set(CMAKE_SYSTEM_VERSION "${_FOUND_WINSDK_VERSION}" CACHE STRING "Windows SDK version" FORCE)
    message(STATUS "sdk_windows: Windows SDK version: ${CMAKE_SYSTEM_VERSION}")
endif()
