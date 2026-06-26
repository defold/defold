if(DEFINED DEFOLD_FUNCTIONS_CMAKE_INCLUDED)
    return()
endif()
set(DEFOLD_FUNCTIONS_CMAKE_INCLUDED ON)

message(DEBUG "functions.cmake:")

# Remember where the Defold CMake modules live (scripts/cmake)
# This is evaluated when this file is included, before any toolchain
# can override CMAKE_MODULE_PATH. Functions can then rely on it.
if(NOT DEFINED DEFOLD_CMAKE_DIR)
    set(DEFOLD_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Defold CMake modules dir")
endif()

# Verbose logging helper. Uses STATUS when DEFOLD_VERBOSE=ON, else DEBUG.
function(defold_log MSG)
    if(DEFOLD_VERBOSE)
        message(STATUS "${MSG}")
    else()
        message(DEBUG "${MSG}")
    endif()
endfunction()

function(defold_java_bindings_env OUT_VAR)
    set(_env
        "DEFOLD_HOME=${DEFOLD_HOME}"
        "DYNAMO_HOME=${DEFOLD_SDK_ROOT}")

    if(DEFOLD_BINDGEN_CLANG)
        list(APPEND _env "CLANG=${DEFOLD_BINDGEN_CLANG}")
    endif()
    if(DEFOLD_BINDGEN_CLANGPP)
        list(APPEND _env "CLANGPP=${DEFOLD_BINDGEN_CLANGPP}")
    endif()

    set(${OUT_VAR} ${_env} PARENT_SCOPE)
endfunction()

function(defold_java_bindings_defines OUT_VAR)
    set(_defines)

    if(TARGET_PLATFORM MATCHES "android$")
        list(APPEND _defines "DM_PLATFORM_ANDROID")
    elseif(TARGET_PLATFORM MATCHES "linux$")
        list(APPEND _defines "DM_PLATFORM_LINUX")
    elseif(TARGET_PLATFORM MATCHES "ios$")
        list(APPEND _defines "DM_PLATFORM_IOS")
    elseif(TARGET_PLATFORM MATCHES "macos$")
        list(APPEND _defines "DM_PLATFORM_MACOS")
    elseif(TARGET_PLATFORM MATCHES "xbone$")
        list(APPEND _defines "DM_PLATFORM_VENDOR" "DM_PLATFORM_XBOX")
    elseif(TARGET_PLATFORM MATCHES "win32$")
        list(APPEND _defines "DM_PLATFORM_WINDOWS")
    elseif(TARGET_PLATFORM MATCHES "web$")
        list(APPEND _defines "DM_PLATFORM_HTML5")
    elseif(TARGET_PLATFORM MATCHES "nx64$")
        list(APPEND _defines "DM_PLATFORM_VENDOR" "DM_PLATFORM_SWITCH")
    elseif(TARGET_PLATFORM MATCHES "ps4$|ps5$")
        list(APPEND _defines "DM_PLATFORM_VENDOR" "DM_PLATFORM_PLAYSTATION")
    endif()

    set(_args)
    foreach(_define IN LISTS _defines)
        list(APPEND _args "--define" "${_define}")
    endforeach()

    set(${OUT_VAR} ${_args} PARENT_SCOPE)
endfunction()

# Collect packaged toolchain roots given DEFOLD_SDK_ROOT/ext/SDKs.
# Adds the directory itself and its immediate child directories.
# Usage:
#   defold_collect_packaged_roots("${DEFOLD_SDK_ROOT}/ext/SDKs" OUT_LIST)
function(defold_collect_packaged_roots SDKS_DIR OUT_LIST)
    set(_roots)
    if(SDKS_DIR AND EXISTS "${SDKS_DIR}")
        list(APPEND _roots "${SDKS_DIR}")
        file(GLOB _child_paths "${SDKS_DIR}/*")
        foreach(_p IN LISTS _child_paths)
            if(IS_DIRECTORY "${_p}")
                list(APPEND _roots "${_p}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _roots)
    endif()
    set(${OUT_LIST} "${_roots}" PARENT_SCOPE)
endfunction()

# Create a C++ file implementing dmExportedSymbols() that calls a list of
# exported C symbols provided via the CMake list.
# The file content is generated from exported_symbols.in.cpp.
#
# Usage:
#   include(functions)
#   # provide symbols via list variable
#   set(MYSYMBOLS Foo;Bar;Baz)
#   defold_create_exported_symbols_file(${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp "${MYSYMBOLS}")

function(defold_create_exported_symbols_file OUT_PATH SYMBOL_LIST)
    set(_decls "")
    set(_calls "")

    # Symbol list resolution: use provided semicolon-separated list
    set(_SYMBOLS "${SYMBOL_LIST}")

    foreach(sym IN LISTS _SYMBOLS)
        # Skip empty list items
        if(NOT sym)
            continue()
        endif()
        string(STRIP "${sym}" sym)
        if(NOT sym)
            continue()
        endif()
        string(APPEND _decls "extern \"C\" void ${sym}();\n")
        string(APPEND _calls "    ${sym}();\n")
    endforeach()

    # Variables consumed by the template
    set(DM_EXPORTED_SYMBOL_DECLS "${_decls}")
    set(DM_EXPORTED_SYMBOL_CALLS "${_calls}")

    # Locate the template robustly. Prefer the scripts/cmake folder (where
    # this module lives), since toolchains may override CMAKE_MODULE_PATH.
    set(_DEFOLD_EXPORTED_TPL "")
    set(_tpl_candidate "${DEFOLD_CMAKE_DIR}/exported_symbols.in.cpp")
    if(EXISTS "${_tpl_candidate}")
        set(_DEFOLD_EXPORTED_TPL "${_tpl_candidate}")
    else()
        # Fall back to CMAKE_MODULE_PATH and default search paths
        find_file(_DEFOLD_EXPORTED_TPL "exported_symbols.in.cpp" HINTS ${CMAKE_MODULE_PATH})
    endif()
    if(NOT _DEFOLD_EXPORTED_TPL)
        message(FATAL_ERROR "Could not find exported_symbols.in.cpp. Looked in ${CMAKE_CURRENT_LIST_DIR} and CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}")
    endif()

    # Ensure output directory exists
    get_filename_component(_outdir "${OUT_PATH}" DIRECTORY)
    if(_outdir)
        file(MAKE_DIRECTORY "${_outdir}")
    endif()

    configure_file("${_DEFOLD_EXPORTED_TPL}" "${OUT_PATH}" @ONLY)
    unset(DM_EXPORTED_SYMBOL_DECLS)
    unset(DM_EXPORTED_SYMBOL_CALLS)
endfunction()

include(functions_app)
include(functions_bullet)
include(functions_codesigning)
include(functions_csharp)
include(functions_embed)
include(functions_feature_sources)
include(functions_glfw)
include(functions_graphics)
include(functions_libs)
include(functions_lua)
include(functions_opengl)
include(functions_platform)
include(functions_protoc)
include(functions_sdk)
include(functions_test)
include(functions_testserver)
include(functions_xcode)
