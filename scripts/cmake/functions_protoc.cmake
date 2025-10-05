defold_log("functions_protoc.cmake:")

# Generate C++ sources from a .proto file using protoc with the Defold DDF plugin.
#
# Usage:
#   include(functions_protoc)
#   # Generate into a specific .cpp (header path auto-derived)
#   defold_protoc_gen_cpp(
#     "${CMAKE_CURRENT_BINARY_DIR}/input_ddf.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/proto/input/input_ddf.proto"
#     INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/proto" "${DEFOLD_SDK_ROOT}/share/proto"
#   )
#   # Then add the generated .pb.cc to your target sources.
#
# Notes:
# - OUT_CPP must point to the desired .cpp file path; the header will be
#   generated in the same directory with .h (same base name).
# - INCLUDES accepts a CMake list of include directories to pass as -I to protoc.
# - DEFOLD_SDK_ROOT/share/proto and DEFOLD_SDK_ROOT/ext/include are appended if defined.
#
function(defold_protoc_gen_cpp OUT_CPP SRC_PROTO)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs INCLUDES)
    cmake_parse_arguments(DPC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT OUT_CPP OR NOT SRC_PROTO)
        message(FATAL_ERROR "defold_protoc_gen_cpp: require OUT_CPP and SRC_PROTO")
    endif()

    # Derive output directory and header name
    get_filename_component(_out_dir "${OUT_CPP}" DIRECTORY)
    get_filename_component(_src_abs "${SRC_PROTO}" ABSOLUTE)
    get_filename_component(_src_name_we "${SRC_PROTO}" NAME_WE)

    # DDF plugin emits <name>.cpp and <name>.h
    set(_out_h "${_out_dir}/${_src_name_we}.h")
    set(_out_cc "${_out_dir}/${_src_name_we}.cpp")

    # Build include flags: user-specified + SDK defaults + proto's parent dir
    get_filename_component(_src_dir "${_src_abs}" DIRECTORY)
    set(_inc_dirs ${_src_dir})
    if(DPC_INCLUDES)
        list(APPEND _inc_dirs ${DPC_INCLUDES})
    endif()
    if(DEFOLD_SDK_ROOT)
        list(APPEND _inc_dirs "${DEFOLD_SDK_ROOT}/share/proto" "${DEFOLD_SDK_ROOT}/ext/include")
    endif()
    list(REMOVE_DUPLICATES _inc_dirs)

    set(_inc_flags)
    foreach(_inc IN LISTS _inc_dirs)
        list(APPEND _inc_flags -I "${_inc}")
    endforeach()

    file(MAKE_DIRECTORY "${_out_dir}")

    # Resolve DDF plugin path
    if(NOT DEFOLD_SDK_ROOT)
        message(FATAL_ERROR "defold_protoc_gen_cpp: DEFOLD_SDK_ROOT must be set to locate ddfc_cxx plugin")
    endif()
    if(WIN32)
        set(_ddf_plugin_path "${DEFOLD_SDK_ROOT}/bin/ddfc_cxx.bat")
    else()
        set(_ddf_plugin_path "${DEFOLD_SDK_ROOT}/bin/ddfc_cxx")
    endif()
    if(NOT EXISTS "${_ddf_plugin_path}")
        message(FATAL_ERROR "defold_protoc_gen_cpp: ddf plugin not found at ${_ddf_plugin_path}. Check DEFOLD_SDK_ROOT.")
    endif()

    # Build plugin argument as a single token so paths with spaces work.
    if(WIN32)
        file(TO_NATIVE_PATH "${_ddf_plugin_path}" _ddf_plugin_native)
        set(_ddf_plugin_arg "--plugin=protoc-gen-ddf=${_ddf_plugin_native}")
    else()
        set(_ddf_plugin_arg "--plugin=protoc-gen-ddf=${_ddf_plugin_path}")
    endif()

    set(_PROTOC_BIN protoc)
    if(DEFINED DEFOLD_PROTOC_EXECUTABLE AND EXISTS "${DEFOLD_PROTOC_EXECUTABLE}")
        set(_PROTOC_BIN "${DEFOLD_PROTOC_EXECUTABLE}")
    endif()

    add_custom_command(
        OUTPUT "${_out_cc}" "${_out_h}"
        COMMAND ${_PROTOC_BIN} ${_ddf_plugin_arg} --ddf_out=${_out_dir} ${_inc_flags} ${_src_abs}
        DEPENDS "${_src_abs}"
        VERBATIM
        COMMENT "Generating DDF C++ from ${SRC_PROTO}"
    )

    # Mark generated
    set_source_files_properties("${_out_cc}" "${_out_h}" PROPERTIES GENERATED TRUE)

    # If caller supplied a different OUT_CPP path, create a custom command to copy/rename
    if(NOT "${OUT_CPP}" STREQUAL "${_out_cc}")
        add_custom_command(
            OUTPUT "${OUT_CPP}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_out_cc}" "${OUT_CPP}"
            DEPENDS "${_out_cc}"
            VERBATIM
            COMMENT "Copying ${_out_cc} to ${OUT_CPP}"
        )
        set_source_files_properties("${OUT_CPP}" PROPERTIES GENERATED TRUE)
    endif()

endfunction()

######################################################################
# Encode a text/binary .proto message file using protoc --encode
#
# Usage:
#   defold_protoc_encode(
#     "${CMAKE_CURRENT_BINARY_DIR}/build/src/test/test.input_bindingc"  # OUT_FILE
#     "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test.input_binding"          # SRC_FILE
#     "${CMAKE_CURRENT_SOURCE_DIR}/proto/input/input_ddf.proto"          # SCHEMA_PROTO
#     "dmInputDDF.InputBinding"                                          # MESSAGE_NAME
#     INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/src/test" "${CMAKE_CURRENT_SOURCE_DIR}/proto"
#   )
#
# Notes:
# - Mirrors engine/input/src/test/wscript:create_protoc_task
# - Adds DEFOLD_SDK_ROOT/share/proto and DEFOLD_SDK_ROOT/ext/include to -I by default
# - Uses a CMake script with execute_process(INPUT_FILE/OUTPUT_FILE) so it works cross-platform
function(defold_protoc_encode OUT_FILE SRC_FILE SCHEMA_PROTO MESSAGE_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs INCLUDES)
    cmake_parse_arguments(DPE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT OUT_FILE OR NOT SRC_FILE OR NOT SCHEMA_PROTO OR NOT MESSAGE_NAME)
        message(FATAL_ERROR "defold_protoc_encode: require OUT_FILE, SRC_FILE, SCHEMA_PROTO, MESSAGE_NAME")
    endif()

    get_filename_component(_src_abs "${SRC_FILE}" ABSOLUTE)
    get_filename_component(_schema_abs "${SCHEMA_PROTO}" ABSOLUTE)
    get_filename_component(_out_abs "${OUT_FILE}" ABSOLUTE)
    get_filename_component(_out_dir "${_out_abs}" DIRECTORY)

    # Build include dirs
    set(_inc_dirs)
    if(DPE_INCLUDES)
        list(APPEND _inc_dirs ${DPE_INCLUDES})
    endif()
    if(DEFOLD_SDK_ROOT)
        list(APPEND _inc_dirs "${DEFOLD_SDK_ROOT}/share/proto" "${DEFOLD_SDK_ROOT}/ext/include")
    endif()
    list(REMOVE_DUPLICATES _inc_dirs)
    # Normalize include paths using REALPATH (removes . and .., resolves symlinks)
    set(_norm_inc_dirs)
    foreach(_d IN LISTS _inc_dirs)
        if(IS_ABSOLUTE "${_d}")
            get_filename_component(_norm_inc "${_d}" REALPATH)
        else()
            get_filename_component(_abs_inc "${_d}" ABSOLUTE)
            get_filename_component(_norm_inc "${_abs_inc}" REALPATH)
        endif()
        list(APPEND _norm_inc_dirs "${_norm_inc}")
    endforeach()
    set(_inc_dirs "${_norm_inc_dirs}")

    # Serialize include list into semicolon list for the helper script
    file(MAKE_DIRECTORY "${_out_dir}")

    # Build a single string with includes to avoid list splitting issues
    set(_inc_joined)
    foreach(_i IN LISTS _inc_dirs)
        list(APPEND _inc_joined -I ${_i})
    endforeach()

    # Build shell command to pipe input into protoc --encode and write output
    # Cross-platform handling for input/output redirection

    file(TO_NATIVE_PATH "${_src_abs}" _src_native)
    file(TO_NATIVE_PATH "${_schema_abs}" _schema_native)
    file(TO_NATIVE_PATH "${_out_abs}" _out_native)

    set(_PROTOC_BIN protoc)
    if(DEFINED DEFOLD_PROTOC_EXECUTABLE AND EXISTS "${DEFOLD_PROTOC_EXECUTABLE}")
        set(_PROTOC_BIN "${DEFOLD_PROTOC_EXECUTABLE}")
    endif()

    add_custom_command(
        OUTPUT "${_out_abs}"
        COMMAND ${_PROTOC_BIN} --encode=${MESSAGE_NAME} ${_inc_joined} ${_schema_native} < ${_src_native} > ${_out_native}
        DEPENDS "${_src_abs}" "${_schema_abs}"
        VERBATIM
        COMMENT "Encoding ${_src_abs} as ${MESSAGE_NAME} -> ${_out_abs}"
    )

    set_source_files_properties("${_out_abs}" PROPERTIES GENERATED TRUE)
endfunction()

# Generate Python bindings from a .proto file using protoc.
#
# Usage:
#   defold_protoc_gen_py(
#     "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf_pb2.py"
#     "${CMAKE_CURRENT_SOURCE_DIR}/proto/input/input_ddf.proto"
#     INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/proto" "${DEFOLD_SDK_ROOT}/share/proto"
#   )
#
# Notes:
# - OUT_PY is the desired output module path, typically ending with _pb2.py.
# - The actual generated file name from protoc is <name>_pb2.py. If OUT_PY
#   does not match that, a copy step is added.
# - An empty __init__.py is also created in the output directory so that
#   Python treats it as a package (mirroring waf_ddf.py behavior).

function(defold_protoc_gen_py OUT_PY SRC_PROTO)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs INCLUDES)
    cmake_parse_arguments(DPP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT OUT_PY OR NOT SRC_PROTO)
        message(FATAL_ERROR "defold_protoc_gen_py: require OUT_PY and SRC_PROTO")
    endif()

    get_filename_component(_out_dir "${OUT_PY}" DIRECTORY)
    get_filename_component(_src_abs "${SRC_PROTO}" ABSOLUTE)
    get_filename_component(_src_name_we "${SRC_PROTO}" NAME_WE)

    set(_gen_py "${_out_dir}/${_src_name_we}_pb2.py")
    set(_init_py "${_out_dir}/__init__.py")

    # Build include flags: user-specified + SDK defaults + proto's parent dir
    get_filename_component(_src_dir "${_src_abs}" DIRECTORY)
    set(_inc_dirs ${_src_dir})
    if(DPP_INCLUDES)
        list(APPEND _inc_dirs ${DPP_INCLUDES})
    endif()
    if(DEFOLD_SDK_ROOT)
        list(APPEND _inc_dirs "${DEFOLD_SDK_ROOT}/share/proto" "${DEFOLD_SDK_ROOT}/ext/include")
    endif()
    list(REMOVE_DUPLICATES _inc_dirs)

    set(_inc_flags)
    foreach(_inc IN LISTS _inc_dirs)
        list(APPEND _inc_flags -I "${_inc}")
    endforeach()

    file(MAKE_DIRECTORY "${_out_dir}")

    # Generate both _pb2.py and ensure __init__.py in one rule so either output triggers it
    set(_PROTOC_BIN protoc)
    if(DEFINED DEFOLD_PROTOC_EXECUTABLE AND EXISTS "${DEFOLD_PROTOC_EXECUTABLE}")
        set(_PROTOC_BIN "${DEFOLD_PROTOC_EXECUTABLE}")
    endif()

    add_custom_command(
        OUTPUT "${_gen_py}" "${_init_py}"
        COMMAND ${_PROTOC_BIN} --python_out=${_out_dir} ${_inc_flags} ${_src_abs}
        COMMAND ${CMAKE_COMMAND} -E touch "${_init_py}"
        DEPENDS "${_src_abs}"
        VERBATIM
        COMMENT "Generating Python from ${SRC_PROTO} and ensuring package __init__.py"
    )
    set_source_files_properties("${_gen_py}" "${_init_py}" PROPERTIES GENERATED TRUE)

    # Optional copy if OUT_PY differs from protoc default
    if(NOT "${OUT_PY}" STREQUAL "${_gen_py}")
        add_custom_command(
            OUTPUT "${OUT_PY}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_gen_py}" "${OUT_PY}"
            DEPENDS "${_gen_py}"
            VERBATIM
            COMMENT "Copying ${_gen_py} to ${OUT_PY}"
        )
        set_source_files_properties("${OUT_PY}" PROPERTIES GENERATED TRUE)
    endif()

endfunction()
