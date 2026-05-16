cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(GUI_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(GUI_ROOT "${GUI_TEST_ROOT}/../.." ABSOLUTE)
set(GUI_SRC_DIR "${GUI_ROOT}/src")
set(GUI_TEST_RUNTIME_DIR "${GUI_ROOT}/build/src/test")
set(GUI_TEST_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(GUI_TEST_PROTO "${GUI_TEST_ROOT}/test_gui_ddf.proto")
set(GUI_TEST_PROTO_CC "${GUI_TEST_GEN_DIR}/test_gui_ddf.cpp")
defold_protoc_gen_cpp("${GUI_TEST_PROTO_CC}" "${GUI_TEST_PROTO}"
  INCLUDES "${GUI_TEST_ROOT}" "${DEFOLD_SDK_ROOT}/share/proto")

set(GUI_TEST_BUG352_CPP "${GUI_TEST_GEN_DIR}/bug352.lua.embed.cpp")
set(GUI_TEST_BUG352_H "${GUI_TEST_GEN_DIR}/bug352.lua.embed.h")
defold_embed_to_header(
  INPUT "${GUI_TEST_ROOT}/bug352.lua"
  OUTPUT_HEADER "${GUI_TEST_BUG352_H}"
  OUTPUT_CPP "${GUI_TEST_BUG352_CPP}"
  SYMBOL "BUG352_LUA")

set(GUI_TEST_PARTICLE_PROTO "${DEFOLD_HOME}/engine/particle/proto/particle/particle_ddf.proto")
set(GUI_TEST_PARTICLEFX_OUTPUTS)
foreach(_particlefx IN ITEMS once.particlefx once_three_emitters.particlefx)
  get_filename_component(_base "${_particlefx}" NAME_WE)
  set(_out "${GUI_TEST_RUNTIME_DIR}/${_base}.particlefxc")
  defold_protoc_encode(
    "${_out}"
    "${GUI_TEST_ROOT}/${_particlefx}"
    "${GUI_TEST_PARTICLE_PROTO}"
    "dmParticleDDF.ParticleFX"
    INCLUDES "${DEFOLD_HOME}/engine/particle/proto/particle" "${DEFOLD_HOME}/engine/particle/proto")
  list(APPEND GUI_TEST_PARTICLEFX_OUTPUTS "${_out}")
endforeach()
add_custom_target(gui_test_data DEPENDS ${GUI_TEST_PARTICLEFX_OUTPUTS})

set(GUI_TEST_LIBS
  testmain
  ddf
  dlib
  zip
  mbedtls
  profile_null
  platform_null
  graphics_null
  image
  graphics_proto
  extension
  resource
  particle
  rig
  hid_null
  lua
  script
  gameobject)

function(gui_configure_test target gui_lib run_test)
  target_include_directories(${target} PRIVATE
    "${GUI_TEST_ROOT}"
    "${GUI_SRC_DIR}"
    "${GUI_TEST_GEN_DIR}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    ${GUI_TEST_LIBS}
    font
    ${gui_lib})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_app(${target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}"
    library_sys.js
    library_script.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${target} ${run_test} "${GUI_ROOT}"
    STAGE_FILES
      "build/src/test" "build/src/test")
  add_dependencies(${target} gui_test_data)
  if(TARGET run_${target})
    add_dependencies(run_${target} gui_test_data)
  endif()
endfunction()

defold_add_executable(test_gui
  "${GUI_TEST_ROOT}/test_gui.cpp"
  "${GUI_TEST_PROTO_CC}"
  "${GUI_TEST_BUG352_CPP}")
gui_configure_test(test_gui gui ON)

defold_add_executable(test_gui_script "${GUI_TEST_ROOT}/test_gui_script.cpp")
gui_configure_test(test_gui_script gui ON)

defold_add_executable(test_gui_clipping "${GUI_TEST_ROOT}/test_gui_clipping.cpp")
gui_configure_test(test_gui_clipping gui ON)

defold_add_executable(test_gui_null
  "${GUI_TEST_ROOT}/test_gui.cpp"
  "${GUI_TEST_PROTO_CC}"
  "${GUI_TEST_BUG352_CPP}")
gui_configure_test(test_gui_null gui_null OFF)

defold_add_executable(test_gui_script_null "${GUI_TEST_ROOT}/test_gui_script.cpp")
gui_configure_test(test_gui_script_null gui_null OFF)

defold_add_executable(test_gui_clipping_null "${GUI_TEST_ROOT}/test_gui_clipping.cpp")
gui_configure_test(test_gui_clipping_null gui_null OFF)
