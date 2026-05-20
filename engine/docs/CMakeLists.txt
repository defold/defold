cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_docs LANGUAGES ${DEFOLD_LANGUAGE_LIST})

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(DOCS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DOCS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(DOCS_APIDOC_DIR "${DOCS_BUILD_DIR}/apidoc")
set(DOCS_CONVERTED_DIR "${DOCS_BUILD_DIR}/converted")
set(DOCS_STAMP_DIR "${DOCS_BUILD_DIR}/stamps")
set(DOCS_PROTO_PY_DIR "${DOCS_BUILD_DIR}/python")
set(DOCS_PROTO_PY "${DOCS_PROTO_PY_DIR}/script_doc_ddf_pb2.py")
set(DOCS_SHARE_DIR "${DEFOLD_SDK_ROOT}/share")
set(DOCS_OUTPUT_DIR "${DOCS_SHARE_DIR}/doc")
set(DOCS_ARCHIVE "${DOCS_SHARE_DIR}/ref-doc.zip")
set(DOCS_OUTPUT_MANIFEST "${DOCS_BUILD_DIR}/docs_outputs.txt")

set(DEFOLD_DOCS_FORMATS "sdoc;json;script_api;lua" CACHE STRING "Semicolon-separated docs output formats")
set(DEFOLD_DOCS_RUN_TESTS ON CACHE BOOL "Run script_doc.py tests when building docs")

set(_DOCS_ALL_FORMATS sdoc json script_api lua)
set(_DOCS_SOURCE_EXTENSIONS .h .hpp .cpp .cs .c .doc_h .proto .apidoc)

defold_protoc_gen_py(
  "${DOCS_PROTO_PY}"
  "${DEFOLD_HOME}/engine/script/src/script/script_doc_ddf.proto")

macro(docs_add_source root source)
  get_filename_component(_docs_root_abs "${root}" ABSOLUTE)
  get_filename_component(_docs_source_abs "${source}" ABSOLUTE)
  list(FIND _DOCS_SOURCE_FILES "${_docs_source_abs}" _docs_existing_source)
  if(_docs_existing_source EQUAL -1)
    file(RELATIVE_PATH _docs_rel "${_docs_root_abs}" "${_docs_source_abs}")
    string(REPLACE "/" "-" _docs_key "${_docs_rel}")
    list(FIND _DOCS_KEYS "${_docs_key}" _docs_existing_key)
    if(NOT _docs_existing_key EQUAL -1)
      message(FATAL_ERROR "Duplicate docs output key '${_docs_key}' for '${_docs_source_abs}'")
    endif()
    list(APPEND _DOCS_SOURCE_FILES "${_docs_source_abs}")
    list(APPEND _DOCS_SOURCE_ROOTS "${_docs_root_abs}")
    list(APPEND _DOCS_KEYS "${_docs_key}")
  endif()
endmacro()

macro(docs_add_inputs root_rel)
  set(_docs_root "${DEFOLD_HOME}/${root_rel}")
  foreach(_docs_relpath ${ARGN})
    set(_docs_fullpath "${_docs_root}/${_docs_relpath}")
    if(IS_DIRECTORY "${_docs_fullpath}" OR "${_docs_relpath}" STREQUAL "")
      file(GLOB_RECURSE _docs_candidates CONFIGURE_DEPENDS LIST_DIRECTORIES false "${_docs_fullpath}/*")
      list(SORT _docs_candidates)
      foreach(_docs_candidate IN LISTS _docs_candidates)
        get_filename_component(_docs_ext "${_docs_candidate}" EXT)
        list(FIND _DOCS_SOURCE_EXTENSIONS "${_docs_ext}" _docs_ext_index)
        if(NOT _docs_ext_index EQUAL -1)
          docs_add_source("${_docs_root}" "${_docs_candidate}")
        endif()
      endforeach()
    elseif(EXISTS "${_docs_fullpath}")
      docs_add_source("${_docs_root}" "${_docs_fullpath}")
    endif()
  endforeach()
endmacro()

set(DOCS_EDITOR_APIDOC "${DOCS_APIDOC_DIR}/editor.apidoc.apidoc")
file(GLOB_RECURSE _DOCS_EDITOR_DOC_SOURCES CONFIGURE_DEPENDS
  "${DEFOLD_HOME}/editor/src/docs/*"
  "${DEFOLD_HOME}/editor/src/clj/editor/editor_extensions/*.clj")

add_custom_command(
  OUTPUT "${DOCS_EDITOR_APIDOC}"
  COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
    editor
    --defold-root "${DEFOLD_HOME}"
    --output "${DOCS_EDITOR_APIDOC}"
  DEPENDS
    "${DOCS_ROOT}/build_docs.py"
    "${DEFOLD_HOME}/editor/scripts/bundle.py"
    ${_DOCS_EDITOR_DOC_SOURCES}
  COMMENT "Generating editor API docs"
  VERBATIM)

set(_DOCS_APIDOC_OUTPUTS "${DOCS_EDITOR_APIDOC}")
set(_DOCS_APIDOC_KEYS "editor.apidoc")

docs_add_inputs("engine/crash/src" "script_crash.cpp")
docs_add_inputs("engine/ddf/src" "dmsdk")
docs_add_inputs("engine/dlib/src" "dmsdk")
docs_add_inputs("engine/dlib/src" "cs/dmsdk")
docs_add_inputs("engine/engine/proto" "engine/engine_ddf.proto")
docs_add_inputs("engine/engine/src" "engine_doc.h" "script/script_engine.cpp")
docs_add_inputs("engine/engine/src" "dmsdk")
docs_add_inputs("engine/extension/src" "dmsdk")
docs_add_inputs("engine/extension/src" "cs/dmsdk")
docs_add_inputs("engine/font/src" "dmsdk")
docs_add_inputs("engine/gameobject/proto" "gameobject/gameobject_ddf.proto")
docs_add_inputs("engine/gameobject/src/gameobject" "gameobject_script.cpp")
docs_add_inputs("engine/gameobject/src" "dmsdk")
docs_add_inputs("engine/gamesys/proto" "gamesys")
docs_add_inputs("engine/gamesys/src" "dmsdk")
docs_add_inputs("engine/gamesys/src/gamesys" "scripts")
docs_add_inputs("engine/gamesys/src" "gamesys/box2d")
docs_add_inputs("engine/graphics/src" "dmsdk")
docs_add_inputs("engine/gui/src" "gui_script.cpp")
docs_add_inputs("engine/gui/src" "dmsdk")
docs_add_inputs("engine/hid/src" "dmsdk")
docs_add_inputs("engine/liveupdate" "src")
docs_add_inputs("engine/lua/src" "")
docs_add_inputs("engine/lua/src" "dmsdk")
docs_add_inputs("engine/lua/src" "cs/dmsdk")
docs_add_inputs("engine/physics/src" "dmsdk")
docs_add_inputs("engine/platform/src" "dmsdk")
docs_add_inputs("engine/profiler/src" "profiler.cpp")
docs_add_inputs("engine/render/proto" "render")
docs_add_inputs("engine/render/src" "render/render_script.cpp" "render/render_script_camera.cpp")
docs_add_inputs("engine/render/src" "dmsdk")
docs_add_inputs("engine/resource/src" "dmsdk")
docs_add_inputs("engine/rig/src" "dmsdk")
docs_add_inputs("engine/script/src" "dmsdk")
docs_add_inputs("engine/script/src" "luasocket")
docs_add_inputs("engine/script/src" "script")
docs_add_inputs("engine/script" "src")
docs_add_inputs("engine/sdk/src" "dmsdk")
docs_add_inputs("engine/sdk/src" "cs/dmsdk")
docs_add_inputs("engine/sound/src" "dmsdk")

list(LENGTH _DOCS_SOURCE_FILES _DOCS_SOURCE_COUNT)
math(EXPR _DOCS_SOURCE_LAST "${_DOCS_SOURCE_COUNT} - 1")
foreach(_docs_index RANGE 0 ${_DOCS_SOURCE_LAST})
  list(GET _DOCS_SOURCE_FILES ${_docs_index} _docs_source)
  list(GET _DOCS_KEYS ${_docs_index} _docs_key)
  set(_docs_apidoc "${DOCS_APIDOC_DIR}/${_docs_key}.apidoc")
  add_custom_command(
    OUTPUT "${_docs_apidoc}"
    COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
      extract
      --defold-root "${DEFOLD_HOME}"
      --source "${_docs_source}"
      --output "${_docs_apidoc}"
    DEPENDS
      "${DOCS_ROOT}/build_docs.py"
      "${_docs_source}"
    COMMENT "Extracting API docs from ${_docs_source}"
    VERBATIM)
  list(APPEND _DOCS_APIDOC_OUTPUTS "${_docs_apidoc}")
  list(APPEND _DOCS_APIDOC_KEYS "${_docs_key}")
endforeach()

set(_DOCS_CONVERTED_OUTPUTS)
list(LENGTH _DOCS_APIDOC_OUTPUTS _DOCS_APIDOC_COUNT)
math(EXPR _DOCS_APIDOC_LAST "${_DOCS_APIDOC_COUNT} - 1")
foreach(_docs_index RANGE 0 ${_DOCS_APIDOC_LAST})
  list(GET _DOCS_APIDOC_OUTPUTS ${_docs_index} _docs_apidoc)
  list(GET _DOCS_APIDOC_KEYS ${_docs_index} _docs_key)
  set(_docs_outputs)
  foreach(_docs_format IN LISTS DEFOLD_DOCS_FORMATS)
    list(FIND _DOCS_ALL_FORMATS "${_docs_format}" _docs_format_index)
    if(_docs_format_index EQUAL -1)
      message(FATAL_ERROR "Unknown docs format '${_docs_format}' in DEFOLD_DOCS_FORMATS")
    endif()
    list(APPEND _docs_outputs "${DOCS_CONVERTED_DIR}/${_docs_key}_doc.${_docs_format}")
  endforeach()
  add_custom_command(
    OUTPUT ${_docs_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
      convert
      --docs-dir "${DOCS_ROOT}"
      --input "${_docs_apidoc}"
      --output-dir "${DOCS_CONVERTED_DIR}"
      --key "${_docs_key}"
      --formats ${DEFOLD_DOCS_FORMATS}
      --pythonpath "${DOCS_PROTO_PY_DIR}"
    DEPENDS
      "${_docs_apidoc}"
      "${DOCS_PROTO_PY}"
      "${DOCS_ROOT}/build_docs.py"
      "${DOCS_ROOT}/script_doc.py"
    COMMENT "Converting API doc ${_docs_key}"
    VERBATIM)
  list(APPEND _DOCS_CONVERTED_OUTPUTS ${_docs_outputs})
endforeach()

file(WRITE "${DOCS_OUTPUT_MANIFEST}" "")
foreach(_docs_output IN LISTS _DOCS_CONVERTED_OUTPUTS)
  get_filename_component(_docs_output_name "${_docs_output}" NAME)
  file(APPEND "${DOCS_OUTPUT_MANIFEST}" "${_docs_output}|${_docs_output_name}\n")
endforeach()

set(DOCS_SYNC_STAMP "${DOCS_STAMP_DIR}/sync.stamp")
add_custom_command(
  OUTPUT "${DOCS_SYNC_STAMP}"
  COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
    sync
    --manifest "${DOCS_OUTPUT_MANIFEST}"
    --output-dir "${DOCS_OUTPUT_DIR}"
    --all-formats ${_DOCS_ALL_FORMATS}
    --stamp "${DOCS_SYNC_STAMP}"
  DEPENDS
    ${_DOCS_CONVERTED_OUTPUTS}
    "${DOCS_OUTPUT_MANIFEST}"
    "${DOCS_ROOT}/build_docs.py"
  COMMENT "Installing API docs"
  VERBATIM)

add_custom_command(
  OUTPUT "${DOCS_ARCHIVE}"
  COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
    zip
    --input-dir "${DOCS_OUTPUT_DIR}"
    --archive "${DOCS_ARCHIVE}"
    --archive-root "${DOCS_SHARE_DIR}"
  DEPENDS
    "${DOCS_SYNC_STAMP}"
    "${DOCS_ROOT}/build_docs.py"
  COMMENT "Packaging API docs"
  VERBATIM)

set(_DOCS_BUILD_DEPS "${DOCS_ARCHIVE}")
if(DEFOLD_DOCS_RUN_TESTS)
  set(DOCS_TEST_STAMP "${DOCS_STAMP_DIR}/test_script_doc.stamp")
  add_custom_command(
    OUTPUT "${DOCS_TEST_STAMP}"
    COMMAND "${Python3_EXECUTABLE}" "${DOCS_ROOT}/build_docs.py"
      test
      --docs-dir "${DOCS_ROOT}"
      --pythonpath "${DOCS_PROTO_PY_DIR}"
      --stamp "${DOCS_TEST_STAMP}"
    DEPENDS
      "${DOCS_PROTO_PY}"
      "${DOCS_ROOT}/build_docs.py"
      "${DOCS_ROOT}/script_doc.py"
      "${DOCS_ROOT}/test_script_doc.py"
    WORKING_DIRECTORY "${DOCS_ROOT}"
    COMMENT "Running script_doc tests"
    VERBATIM)
  list(APPEND _DOCS_BUILD_DEPS "${DOCS_TEST_STAMP}")
endif()

add_custom_target(build_docs DEPENDS ${_DOCS_BUILD_DEPS})
