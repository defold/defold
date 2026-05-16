# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_gamesys LANGUAGES C CXX)

# Log domain for this module
add_compile_definitions(DLIB_LOG_DOMAIN="GAMESYS")

set(GS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GS_SRC_DIR "${GS_ROOT}/src/gamesys")
set(GS_DMSDK_SRC_DIR "${GS_ROOT}/src")
set(GS_PROTO_DIR "${GS_ROOT}/proto/gamesys")

# ========= Protobuf (DDF) =========
# Mirror wscript: include all proto files under proto/gamesys except model_ddf.proto
file(GLOB _ALL_PROTO_FILES "${GS_PROTO_DIR}/*.proto")
set(GS_PROTO_FILES)
foreach(_pf IN LISTS _ALL_PROTO_FILES)
  if(NOT _pf MATCHES "/model_ddf\\.proto$")
    list(APPEND GS_PROTO_FILES "${_pf}")
  endif()
endforeach()

set(GS_PROTO_CC)
set(GS_PROTO_HEADERS)
foreach(_pf IN LISTS GS_PROTO_FILES)
  get_filename_component(_base "${_pf}" NAME_WE)
  set(_out_cc "${CMAKE_CURRENT_BINARY_DIR}/gamesys/${_base}.pb.cc")
  set(_out_h "${CMAKE_CURRENT_BINARY_DIR}/gamesys/${_base}.h")
  defold_protoc_gen_cpp("${_out_cc}" "${_pf}" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
  list(APPEND GS_PROTO_CC "${_out_cc}")
  list(APPEND GS_PROTO_HEADERS "${_out_h}")
endforeach()

# Generate Python protobuf modules required by content tooling
set(GS_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto/gamesys")
file(MAKE_DIRECTORY "${GS_PROTO_PY_DIR}")
set(GS_PROTO_PY)
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/physics_ddf_pb2.py" "${GS_PROTO_DIR}/physics_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/gamesys_ddf_pb2.py" "${GS_PROTO_DIR}/gamesys_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/gui_ddf_pb2.py" "${GS_PROTO_DIR}/gui_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/tile_ddf_pb2.py" "${GS_PROTO_DIR}/tile_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/label_ddf_pb2.py" "${GS_PROTO_DIR}/label_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/camera_ddf_pb2.py" "${GS_PROTO_DIR}/camera_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/sprite_ddf_pb2.py" "${GS_PROTO_DIR}/sprite_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/texture_set_ddf_pb2.py" "${GS_PROTO_DIR}/texture_set_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/sound_ddf_pb2.py" "${GS_PROTO_DIR}/sound_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/mesh_ddf_pb2.py" "${GS_PROTO_DIR}/mesh_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/atlas_ddf_pb2.py" "${GS_PROTO_DIR}/atlas_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/buffer_ddf_pb2.py" "${GS_PROTO_DIR}/buffer_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/data_ddf_pb2.py" "${GS_PROTO_DIR}/data_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${GS_PROTO_PY_DIR}/model_ddf_pb2.py" "${GS_PROTO_DIR}/model_ddf.proto" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
list(APPEND GS_PROTO_PY
  "${GS_PROTO_PY_DIR}/physics_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/gamesys_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/gui_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/tile_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/label_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/camera_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/sprite_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/texture_set_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/sound_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/mesh_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/atlas_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/buffer_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/data_ddf_pb2.py"
  "${GS_PROTO_PY_DIR}/model_ddf_pb2.py"
)

add_custom_target(gamesys_proto_py ALL DEPENDS ${GS_PROTO_PY})

set(GAMESYS_PROTO_PY_DIR "${GS_PROTO_PY_DIR}" CACHE INTERNAL "Generated gamesys python protobuf modules")

# ========= gamesys (main) =========

# Mirror ant_glob patterns from wscript:
#   ['*.cpp', 'resources/**/*.cpp', 'components/**/*.cpp', 'scripts/*.cpp', 'fontgen/*.cpp']
file(GLOB GS_TOP_LEVEL
    "${GS_SRC_DIR}/*.cpp"
    "${GS_SRC_DIR}/scripts/*.cpp"
    "${GS_SRC_DIR}/fontgen/*.cpp")
file(GLOB_RECURSE GS_RESOURCES "${GS_SRC_DIR}/resources/*.cpp")
file(GLOB_RECURSE GS_COMPONENTS "${GS_SRC_DIR}/components/*.cpp")
set(GS_SOURCES ${GS_TOP_LEVEL} ${GS_RESOURCES} ${GS_COMPONENTS})

# Exclude model/rig sources (built as separate libs) using explicit paths for reliability
# Apply remove_from_node_list() exclusions by basename
list(FILTER GS_SOURCES EXCLUDE REGEX "/comp_model\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/comp_model_null\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/res_model\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/res_model_null\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/res_rig_scene\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/res_rig_scene_null\\.cpp$")
list(FILTER GS_SOURCES EXCLUDE REGEX "/script_model\\.cpp$")

# Mirror waf include patterns: do not include Box2D script sources in the
# main gamesys library. They are built as separate libraries below
# (script_box2d, script_box2d_defold). The waf wscript uses 'scripts/*.cpp'
# (non-recursive), so scripts/box2d/*.cpp are not part of gamesys.
list(FILTER GS_SOURCES EXCLUDE REGEX "/scripts/box2d/")

# Choose platform-specific window script
if (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
    list(APPEND GS_SOURCES "${GS_SRC_DIR}/scripts/window/script_window_ios.mm")
elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
    list(APPEND GS_SOURCES "${GS_SRC_DIR}/scripts/window/script_window_android.cpp")
else()
    list(APPEND GS_SOURCES "${GS_SRC_DIR}/scripts/window/script_window_null.cpp")
endif()

# HTTP implementation: JS vs native
if (TARGET_PLATFORM MATCHES "^(js-web|wasm-web|wasm_pthread-web)$")
    list(FILTER GS_SOURCES EXCLUDE REGEX "/scripts/script_http\\.cpp$")
else()
    list(FILTER GS_SOURCES EXCLUDE REGEX "/scripts/script_http_js\\.cpp$")
endif()

defold_add_library(gamesys STATIC ${GS_SOURCES} ${GS_PROTO_CC})
target_include_directories(gamesys PUBLIC
    "${GS_SRC_DIR}"
    "${GS_DMSDK_SRC_DIR}"
    "${GS_ROOT}/src"
    "${GS_ROOT}/proto"
    "${CMAKE_CURRENT_BINARY_DIR}")
defold_target_link_libraries(gamesys "${TARGET_PLATFORM}" SCOPE PUBLIC render)

# ========= gamesys_model =========
set(GS_MODEL_PROTO "${GS_PROTO_DIR}/model_ddf.proto")
set(GS_MODEL_CC "${CMAKE_CURRENT_BINARY_DIR}/gamesys/model_ddf.pb.cc")
set(GS_MODEL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/gamesys/model_ddf.h")
defold_protoc_gen_cpp("${GS_MODEL_CC}" "${GS_MODEL_PROTO}" INCLUDES "${GS_ROOT}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
add_custom_target(gamesys_proto_headers DEPENDS ${GS_PROTO_HEADERS} "${GS_MODEL_HEADER}")

defold_add_library(gamesys_model STATIC
    "${GS_SRC_DIR}/resources/res_model.cpp"
    "${GS_SRC_DIR}/components/comp_model.cpp"
    "${GS_SRC_DIR}/scripts/script_model.cpp"
    "${GS_MODEL_CC}")
target_include_directories(gamesys_model PUBLIC "${GS_SRC_DIR}" "${GS_DMSDK_SRC_DIR}" "${GS_ROOT}/proto" "${CMAKE_CURRENT_BINARY_DIR}")
defold_target_link_libraries(gamesys_model "${TARGET_PLATFORM}" SCOPE PUBLIC render rig)

defold_add_library(gamesys_model_null STATIC
    "${GS_SRC_DIR}/resources/res_model_null.cpp"
    "${GS_SRC_DIR}/components/comp_model_null.cpp")
target_include_directories(gamesys_model_null PUBLIC "${GS_SRC_DIR}" "${GS_DMSDK_SRC_DIR}")

# ========= gamesys_rig =========
defold_add_library(gamesys_rig STATIC "${GS_SRC_DIR}/resources/res_rig_scene.cpp")
target_include_directories(gamesys_rig PUBLIC "${GS_SRC_DIR}" "${GS_DMSDK_SRC_DIR}")
defold_target_link_libraries(gamesys_rig "${TARGET_PLATFORM}" SCOPE PUBLIC rig)

defold_add_library(gamesys_rig_null STATIC "${GS_SRC_DIR}/resources/res_rig_scene_null.cpp")
target_include_directories(gamesys_rig_null PUBLIC "${GS_SRC_DIR}" "${GS_DMSDK_SRC_DIR}")

# ========= script_box2d =========
defold_add_library(script_box2d STATIC
    "${GS_SRC_DIR}/scripts/box2d/script_box2d.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v3/script_box2d_body_v3.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v3/script_box2d_joint_v3.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v3/script_box2d_shape_v3.cpp")
# Default to the Defold-provided Box2D headers
target_include_directories(script_box2d PUBLIC
    "${GS_SRC_DIR}"
    "${GS_DMSDK_SRC_DIR}"
    "${DEFOLD_EXT_INCLUDE_DIR}/box2d"
    "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(script_box2d gamesys_proto_headers)



defold_add_library(script_box2d_defold STATIC
    "${GS_SRC_DIR}/scripts/box2d/script_box2d.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v2/script_box2d_body_v2.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v2/script_box2d_fixture_v2.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v2/script_box2d_joint_v2.cpp"
    "${GS_SRC_DIR}/scripts/box2d/v2/script_box2d_shape_v2.cpp")
target_include_directories(script_box2d_defold PUBLIC
    "${GS_SRC_DIR}"
    "${GS_DMSDK_SRC_DIR}"
    "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold"
    "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(script_box2d_defold gamesys_proto_headers)

# ========= Android Java (optional) =========
if (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
    find_package(Java 21 COMPONENTS Development QUIET)
    if(Java_JAVAC_EXECUTABLE AND Java_JAR_EXECUTABLE)
        file(GLOB_RECURSE GS_JAVA_SRC "${GS_ROOT}/src/java/*.java")
        set(GS_JAR "${CMAKE_CURRENT_BINARY_DIR}/gamesys_android.jar")
        add_custom_command(OUTPUT "${GS_JAR}"
            COMMAND "${Java_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options -classpath "${DEFOLD_ANDROID_JAR}" -d "${CMAKE_CURRENT_BINARY_DIR}/java" ${GS_JAVA_SRC}
            COMMAND "${Java_JAR_EXECUTABLE}" cf "${GS_JAR}" -C "${CMAKE_CURRENT_BINARY_DIR}/java" .
            DEPENDS ${GS_JAVA_SRC}
            VERBATIM)
        add_custom_target(gamesys_android_jar ALL DEPENDS "${GS_JAR}")
        install(FILES "${GS_JAR}" DESTINATION share/java)
    endif()
endif()

# ========= Install =========
install(TARGETS gamesys gamesys_model gamesys_model_null gamesys_rig gamesys_rig_null script_box2d script_box2d_defold
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES "${GS_SRC_DIR}/gamesys.h" DESTINATION include/gamesys)
install(FILES "${GS_SRC_DIR}/components/comp_gui.h" DESTINATION include/gamesys/components)
install(FILES "${GS_SRC_DIR}/fontgen/fontgen.h" DESTINATION include/gamesys/fontgen)
install(FILES ${GS_PROTO_HEADERS} "${GS_MODEL_HEADER}" DESTINATION include/gamesys)
install(DIRECTORY "${GS_ROOT}/src/dmsdk/"
        DESTINATION sdk/include/dmsdk
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp")
install(FILES ${_ALL_PROTO_FILES} DESTINATION share/proto/gamesys)
install(FILES "${GS_ROOT}/src/waf_gamesys.py" DESTINATION lib/python)
install(FILES ${GS_PROTO_PY} DESTINATION lib/python)

# ========= Tests =========
if(BUILD_TESTS)
  add_subdirectory(src/gamesys/test)
endif()
