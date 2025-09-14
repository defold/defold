message("functions.cmake:")

include(functions_graphics)

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

    # Locate the template in CMAKE_MODULE_PATH
    find_file(_DEFOLD_EXPORTED_TPL "exported_symbols.in.cpp" PATHS ${CMAKE_MODULE_PATH} NO_DEFAULT_PATH)
    if(NOT _DEFOLD_EXPORTED_TPL)
        message(FATAL_ERROR "Could not find exported_symbols.in.cpp in CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
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
