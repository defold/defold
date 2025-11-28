if(DEFINED DEFOLD_FUNCTIONS_CONTENT_CMAKE_INCLUDED)
    return()
endif()
set(DEFOLD_FUNCTIONS_CONTENT_CMAKE_INCLUDED ON)

include(functions_protoc)

function(_defold_collect_python_paths OUT_VAR)
    if(NOT OUT_VAR)
        message(FATAL_ERROR "_defold_collect_python_paths: OUT_VAR must be provided")
    endif()

    set(_paths)
    foreach(_base IN LISTS ARGN)
        if(_base)
            list(APPEND _paths "${_base}")
        endif()
    endforeach()

    if(DEFINED ENV{DYNAMO_HOME})
        set(_package_parent "$ENV{DYNAMO_HOME}/lib/python")
        if(EXISTS "${_package_parent}")
            list(APPEND _paths "${_package_parent}")
            file(GLOB_RECURSE _init_files LIST_DIRECTORIES false "${_package_parent}/*/__init__.py")
            foreach(_init_file IN LISTS _init_files)
                get_filename_component(_pkg_dir "${_init_file}" DIRECTORY)
                list(APPEND _paths "${_pkg_dir}")
            endforeach()
        endif()
    endif()

    list(REMOVE_DUPLICATES _paths)
    set(${OUT_VAR} "${_paths}" PARENT_SCOPE)
endfunction()

function(glob_files OUT_VAR DIRECTORIES EXTENSIONS)
    if(NOT OUT_VAR)
        message(FATAL_ERROR "glob_files: OUT_VAR must be provided")
    endif()

    set(_dirs ${DIRECTORIES})
    set(_excludes ${EXTENSIONS})
    set(_files)

    foreach(_dir IN LISTS _dirs)
        if(NOT _dir)
            continue()
        endif()

        set(_pattern "${_dir}")
        if(IS_ABSOLUTE "${_pattern}")
            set(_abs_pattern "${_pattern}")
        else()
            set(_abs_pattern "${CMAKE_CURRENT_LIST_DIR}/${_pattern}")
        endif()

        if(IS_DIRECTORY "${_abs_pattern}")
            set(_glob_pattern "${_abs_pattern}/*")
        else()
            set(_glob_pattern "${_abs_pattern}")
        endif()

        if(_glob_pattern MATCHES "\\*\\*")
            file(GLOB_RECURSE _entries LIST_DIRECTORIES false "${_glob_pattern}")
        else()
            file(GLOB _entries LIST_DIRECTORIES false "${_glob_pattern}")
        endif()

        foreach(_path IN LISTS _entries)
            if(IS_DIRECTORY "${_path}")
                continue()
            endif()

            set(_skip OFF)
            if(_excludes)
                get_filename_component(_ext "${_path}" EXT)
                string(TOLOWER "${_ext}" _ext_lower)
                string(REGEX REPLACE "^\\." "" _ext_trim "${_ext_lower}")

                foreach(_exc IN LISTS _excludes)
                    string(TOLOWER "${_exc}" _exc_lower)
                    string(REGEX REPLACE "^\\." "" _exc_trim "${_exc_lower}")
                    if(_exc_trim STREQUAL _ext_trim)
                        set(_skip ON)
                        break()
                    endif()
                endforeach()
            endif()

            if(_skip)
                continue()
            endif()

            list(APPEND _files "${_path}")
        endforeach()
    endforeach()

    set(${OUT_VAR} "${_files}" PARENT_SCOPE)
endfunction()

set(DEFOLD_PROTO_GAMESYS "${DEFOLD_HOME}/engine/gamesys/src/proto_gamesys.py")
set(DEFOLD_COMPILE_LUASCRIPT "${DEFOLD_CMAKE_DIR}/compile_luascript.py")

function(_defold_register_content_proto_rule EXT OUT_EXT)
    set_property(GLOBAL APPEND PROPERTY DEFOLD_CONTENT_PROTO_RULES "${EXT}|${OUT_EXT}")
endfunction()

function(_defold_register_content_script_rule EXT OUT_EXT)
    set_property(GLOBAL APPEND PROPERTY DEFOLD_CONTENT_SCRIPT_RULES "${EXT}|${OUT_EXT}")
endfunction()

# Reset stored rules each time this module is processed during configuration.
set_property(GLOBAL PROPERTY DEFOLD_CONTENT_PROTO_RULES "")
set_property(GLOBAL PROPERTY DEFOLD_CONTENT_SCRIPT_RULES "")


# Register protobuf content rules mirroring waf proto_compile_task usage.
_defold_register_content_proto_rule(.camera .camerac)
_defold_register_content_proto_rule(.collection .collectionc)
_defold_register_content_proto_rule(.collectionfactory .collectionfactoryc)
_defold_register_content_proto_rule(.collectionproxy .collectionproxyc)
_defold_register_content_proto_rule(.collisionobject .collisionobjectc)
_defold_register_content_proto_rule(.convexshape .convexshapec)
_defold_register_content_proto_rule(.display_profiles .display_profilesc)
_defold_register_content_proto_rule(.factory .factoryc)
_defold_register_content_proto_rule(.gamepads .gamepadsc)
_defold_register_content_proto_rule(.go .goc)
_defold_register_content_proto_rule(.gui .guic)
_defold_register_content_proto_rule(.input_binding .input_bindingc)
_defold_register_content_proto_rule(.label .labelc)
_defold_register_content_proto_rule(.light .lightc)
_defold_register_content_proto_rule(.mesh .meshc)
_defold_register_content_proto_rule(.particlefx .particlefxc)
_defold_register_content_proto_rule(.render .renderc)
_defold_register_content_proto_rule(.render_target .render_targetc)
_defold_register_content_proto_rule(.sound .soundc)
_defold_register_content_proto_rule(.sprite .spritec)
_defold_register_content_proto_rule(.tilegrid .tilemapc)
_defold_register_content_proto_rule(.tilemap .tilemapc)

_defold_register_content_script_rule(.lua .luac)
_defold_register_content_script_rule(.script .scriptc)
_defold_register_content_script_rule(.gui_script .gui_scriptc)
_defold_register_content_script_rule(.render_script .render_scriptc)

# Process one or more content root folders, converting protobuf text assets
# into their binary counterparts using the registered rules above.
function(defold_content OUTPUT_VAR)
    if(NOT OUTPUT_VAR)
        message(FATAL_ERROR "defold_content: OUTPUT_VAR must be provided")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "defold_content: at least one input file is required")
    endif()

    if(NOT EXISTS "${DEFOLD_PROTO_GAMESYS}")
        message(FATAL_ERROR "defold_content: missing proto helper at ${DEFOLD_PROTO_GAMESYS}")
    endif()

    get_property(_proto_rules GLOBAL PROPERTY DEFOLD_CONTENT_PROTO_RULES)
    if(NOT _proto_rules)
        message(FATAL_ERROR "defold_content: No gamesys content rules found!")
    endif()

    get_property(_script_rules GLOBAL PROPERTY DEFOLD_CONTENT_SCRIPT_RULES)

    if(_script_rules AND NOT EXISTS "${DEFOLD_COMPILE_LUASCRIPT}")
        message(FATAL_ERROR "defold_content: missing compile_luascript helper at ${DEFOLD_COMPILE_LUASCRIPT}")
    endif()

    if(NOT Python3_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endif()

    set(_py_root "${CMAKE_CURRENT_BINARY_DIR}/proto")
    # Default output directory mirrors previous behavior (a 'content' subfolder)
    set(_base_out_dir "${CMAKE_CURRENT_BINARY_DIR}/content")
    _defold_collect_python_paths(_default_python_paths "${_py_root}")

    set(_generated_outputs)
    set(_registered_outputs)

    # support optional keyword arguments: PYTHONPATH <path...>, CONTENT_ROOT <dir>, OUT_DIR <dir>
    set(_extra_python_paths)
    set(_content_root_override "")
    set(_out_dir_override "")
    set(_inputs)
    set(_mode "inputs")
    foreach(_arg IN LISTS ARGN)
        if(_arg STREQUAL "PYTHONPATH")
            set(_mode "pythonpath")
            continue()
        elseif(_arg STREQUAL "CONTENT_ROOT")
            set(_mode "contentroot")
            continue()
        elseif(_arg STREQUAL "OUT_DIR")
            set(_mode "outdir")
            continue()
        endif()

        if(_mode STREQUAL "pythonpath")
            list(APPEND _extra_python_paths "${_arg}")
        elseif(_mode STREQUAL "contentroot")
            set(_content_root_override "${_arg}")
            set(_mode "inputs")
        elseif(_mode STREQUAL "outdir")
            set(_out_dir_override "${_arg}")
            set(_mode "inputs")
        else()
            list(APPEND _inputs "${_arg}")
        endif()
    endforeach()

    # Apply OUT_DIR override if provided
    if(_out_dir_override)
        get_filename_component(_base_out_dir "${_out_dir_override}" ABSOLUTE)
    endif()

    foreach(_src IN LISTS _inputs)
        if(NOT _src)
            continue()
        endif()

        get_filename_component(_abs_src "${_src}" ABSOLUTE)
        if(NOT EXISTS "${_abs_src}")
            message(FATAL_ERROR "defold_content: file ${_src} does not exist")
        endif()

        if(_content_root_override)
            get_filename_component(_content_root "${_content_root_override}" ABSOLUTE)
        else()
            get_filename_component(_content_root "${_abs_src}" DIRECTORY)
        endif()
        file(RELATIVE_PATH _rel_path "${_content_root}" "${_abs_src}")
        get_filename_component(_ext_src "${_abs_src}" EXT)
        string(TOLOWER "${_ext_src}" _ext_lower)

        set(_handled OFF)
        if(_script_rules)
            foreach(_rule IN LISTS _script_rules)
                string(REPLACE "|" ";" _parts "${_rule}")
                list(GET _parts 0 _script_ext)
                list(GET _parts 1 _script_out_ext)
                string(TOLOWER "${_script_ext}" _script_ext_lower)
                if(_ext_lower STREQUAL _script_ext_lower)
                    set(_handled ON)
                    string(REGEX REPLACE "\\${_ext_src}$" "${_script_out_ext}" _rel_out "${_rel_path}")
                    set(_out_file "${_base_out_dir}/${_rel_out}")
                    get_filename_component(_out_dir "${_out_file}" DIRECTORY)
                    file(MAKE_DIRECTORY "${_out_dir}")

                    list(FIND _registered_outputs "${_out_file}" _already_script)
                    if(NOT _already_script EQUAL -1)
                        set(_handled ON)
                        break()
                    endif()

                    set(_python_paths ${_default_python_paths} ${_extra_python_paths})
                    list(FILTER _python_paths EXCLUDE REGEX "^$")
                    list(REMOVE_DUPLICATES _python_paths)

                    set(_python_env_paths ${_python_paths})
                    if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
                        list(APPEND _python_env_paths "$ENV{PYTHONPATH}")
                    endif()
                    list(FILTER _python_env_paths EXCLUDE REGEX "^$")
                    list(REMOVE_DUPLICATES _python_env_paths)
                    if(_python_env_paths)
                        list(JOIN _python_env_paths ":" _python_env)
                    else()
                        set(_python_env "")
                    endif()

                    set(_cmd ${CMAKE_COMMAND} -E env "PYTHONPATH=${_python_env}" ${Python3_EXECUTABLE} "${DEFOLD_COMPILE_LUASCRIPT}" --src "${_abs_src}" --out "${_out_file}" --content-root "${_content_root}")
                    add_custom_command(
                        OUTPUT "${_out_file}"
                        COMMAND ${_cmd}
                        DEPENDS "${_abs_src}" "${DEFOLD_COMPILE_LUASCRIPT}"
                        COMMENT "Compiling Lua script ${_rel_path}"
                        VERBATIM)

                    list(APPEND _generated_outputs "${_out_file}")
                    list(APPEND _registered_outputs "${_out_file}")
                    break()
                endif()
            endforeach()
        endif()

        if(_handled)
            continue()
        endif()

        set(_matched_rule OFF)
        foreach(_rule IN LISTS _proto_rules)
            string(REPLACE "|" ";" _parts "${_rule}")
            list(GET _parts 0 _proto_ext)
            list(GET _parts 1 _proto_out_ext)
            string(TOLOWER "${_proto_ext}" _proto_ext_lower)
            if(_ext_lower STREQUAL _proto_ext_lower)
                set(_matched_rule ON)
                set(_out_ext "${_proto_out_ext}")
                break()
            endif()
        endforeach()

        if(NOT _matched_rule)
            if(_ext_lower STREQUAL ".proto")
                message(WARNING "defold_content: no processing rule for ${_src}; skipping")
            endif()
            continue()
        endif()

        string(REGEX REPLACE "\\${_ext_src}$" "${_out_ext}" _rel_out "${_rel_path}")
        set(_out_file "${_base_out_dir}/${_rel_out}")
        get_filename_component(_out_dir "${_out_file}" DIRECTORY)
        file(MAKE_DIRECTORY "${_out_dir}")

        list(FIND _registered_outputs "${_out_file}" _already_proto)
        if(NOT _already_proto EQUAL -1)
            continue()
        endif()

        set(_python_paths ${_default_python_paths} ${_extra_python_paths})
        list(FILTER _python_paths EXCLUDE REGEX "^$")
        list(REMOVE_DUPLICATES _python_paths)

        set(_python_env_paths ${_python_paths})
        if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
            list(APPEND _python_env_paths "$ENV{PYTHONPATH}")
        endif()
        list(FILTER _python_env_paths EXCLUDE REGEX "^$")
        list(REMOVE_DUPLICATES _python_env_paths)
        if(_python_env_paths)
            list(JOIN _python_env_paths ":" _python_env)
        else()
            set(_python_env "")
        endif()

        set(_cmd ${CMAKE_COMMAND} -E env "PYTHONPATH=${_python_env}" ${Python3_EXECUTABLE} "${DEFOLD_PROTO_GAMESYS}" --proto "${_abs_src}" --output "${_out_file}" --compile --content-root "${_content_root}")
        add_custom_command(
            OUTPUT "${_out_file}"
            COMMAND ${_cmd}
            DEPENDS "${_abs_src}" "${DEFOLD_PROTO_GAMESYS}"
            COMMENT "Compiling gamesys content ${_rel_path}"
            VERBATIM)

        set(_deps_file "${_out_file}.deps")
        set(_dep_cmd ${CMAKE_COMMAND} -E env "PYTHONPATH=${_python_env}" ${Python3_EXECUTABLE} "${DEFOLD_PROTO_GAMESYS}" --proto "${_abs_src}" --output "${_deps_file}" --content-root "${_content_root}")
        add_custom_command(
            OUTPUT "${_deps_file}"
            COMMAND ${_dep_cmd}
            DEPENDS "${_abs_src}" "${DEFOLD_PROTO_GAMESYS}"
            COMMENT "Collecting dependencies for ${_rel_path}"
            VERBATIM)

        list(APPEND _generated_outputs "${_out_file}" "${_deps_file}")
        list(APPEND _registered_outputs "${_out_file}" "${_deps_file}")
    endforeach()

    list(REMOVE_DUPLICATES _generated_outputs)
    set(${OUTPUT_VAR} "${_generated_outputs}" PARENT_SCOPE)
endfunction()
