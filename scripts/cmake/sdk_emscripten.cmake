defold_log("sdk_emscripten.cmake:")

# Detect an Emscripten toolchain file, preferring a packaged SDK in
# ${DEFOLD_SDK_ROOT}/ext/SDKs, then falling back to locally installed
# Emscripten via environment variables or common locations.

set(_EM_TOOLCHAIN "")
set(_EMSCRIPTEN_DIR "")

# Helper to add a candidate toolchain from an EMSDK root
function(_defold_add_emsdk_root EMSDK_ROOT)
    if(NOT EMSDK_ROOT)
        return()
    endif()
    # Newer layout
    set(_cand "${EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
    if(EXISTS "${_cand}")
        set(_EM_TOOLCHAIN "${_cand}" PARENT_SCOPE)
        set(_EMSCRIPTEN_DIR "${EMSDK_ROOT}/upstream/emscripten" PARENT_SCOPE)
        return()
    endif()
    # Older layout
    set(_cand2 "${EMSDK_ROOT}/emscripten/cmake/Modules/Platform/Emscripten.cmake")
    if(EXISTS "${_cand2}")
        set(_EM_TOOLCHAIN "${_cand2}" PARENT_SCOPE)
        set(_EMSCRIPTEN_DIR "${EMSDK_ROOT}/emscripten" PARENT_SCOPE)
        return()
    endif()
endfunction()

# 1) Packaged in DEFOLD_SDK_ROOT/ext/SDKs
if(DEFINED DEFOLD_SDK_ROOT)
    set(_SDKS_DIR "${DEFOLD_SDK_ROOT}/ext/SDKs")
    if(EXISTS "${_SDKS_DIR}")
        file(GLOB _emsdk_dirs
            "${_SDKS_DIR}/emsdk*"
            "${_SDKS_DIR}/Emsdk*"
            "${_SDKS_DIR}/emscripten*"
            "${_SDKS_DIR}/Emscripten*"
        )
        foreach(_d IN LISTS _emsdk_dirs)
            if(NOT _EM_TOOLCHAIN)
                _defold_add_emsdk_root("${_d}")
            endif()
        endforeach()
    endif()
endif()

# 2) From environment variables
if(NOT _EM_TOOLCHAIN)
    if(DEFINED ENV{EMSDK})
        _defold_add_emsdk_root("$ENV{EMSDK}")
    endif()
endif()
if(NOT _EM_TOOLCHAIN)
    if(DEFINED ENV{EMSCRIPTEN})
        # EMSCRIPTEN typically points to .../upstream/emscripten
        set(_cand "$ENV{EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake")
        if(EXISTS "${_cand}")
            set(_EM_TOOLCHAIN "${_cand}")
            set(_EMSCRIPTEN_DIR "$ENV{EMSCRIPTEN}")
        endif()
    endif()
endif()

# 3) Common user install locations
if(NOT _EM_TOOLCHAIN)
    if(DEFINED ENV{HOME})
        _defold_add_emsdk_root("$ENV{HOME}/emsdk")
    endif()
endif()

# 4) From emcc on PATH
if(NOT _EM_TOOLCHAIN)
    find_program(_EMCC emcc)
    if(_EMCC)
        get_filename_component(_emcc_dir "${_EMCC}" DIRECTORY)
        # Expect emcc located under .../emscripten
        set(_cand "${_emcc_dir}/cmake/Modules/Platform/Emscripten.cmake")
        if(EXISTS "${_cand}")
            set(_EM_TOOLCHAIN "${_cand}")
            set(_EMSCRIPTEN_DIR "${_emcc_dir}")
        endif()
    endif()
endif()

if(NOT _EM_TOOLCHAIN)
    message(FATAL_ERROR "sdk_emscripten: Failed to locate Emscripten toolchain (Emscripten.cmake). Try setting EMSDK or EMSCRIPTEN environment variables.")
endif()

message(STATUS "sdk_emscripten: Toolchain file: ${_EM_TOOLCHAIN}")
if(_EMSCRIPTEN_DIR)
    message(STATUS "sdk_emscripten: EMSCRIPTEN: ${_EMSCRIPTEN_DIR}")
    set(EMSCRIPTEN "${_EMSCRIPTEN_DIR}" CACHE PATH "Emscripten directory" FORCE)
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${_EM_TOOLCHAIN}" CACHE FILEPATH "Emscripten toolchain file" FORCE)
endif()

