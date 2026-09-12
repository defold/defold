find_package(Python3 REQUIRED COMPONENTS Interpreter)

defold_add_executable(dap_debuggee test_debugger.cpp)
target_include_directories(dap_debuggee PRIVATE ..)
target_link_libraries(dap_debuggee PRIVATE debugger)
defold_target_link_libraries(dap_debuggee "${TARGET_PLATFORM}" profile_null)
defold_register_test_target(dap_debuggee OFF)

add_custom_target(test_debugger_dap
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee>"
  DEPENDS dap_debuggee
  USES_TERMINAL VERBATIM)
defold_register_sequential_test_command(test_debugger_dap
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee>"
  DEPENDS dap_debuggee)

# Exercise the same wire tests against the bundled Lua 5.1 implementation too.
# The regular debugger target links LuaJIT on desktop platforms.
if(TARGET lua)
  defold_add_executable(dap_debuggee_lua test_debugger.cpp ../dap.cpp ../debugger.cpp ../debugger_values.cpp ../debugger_inspect.cpp)
  target_include_directories(dap_debuggee_lua PRIVATE ..)
  target_link_libraries(dap_debuggee_lua PRIVATE lua)
  defold_target_link_libraries(dap_debuggee_lua "${TARGET_PLATFORM}" dlib profile_null)
  defold_target_link_socket(dap_debuggee_lua "${TARGET_PLATFORM}")
  defold_register_test_target(dap_debuggee_lua OFF)
  add_custom_target(test_debugger_dap_lua
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee_lua>"
    DEPENDS dap_debuggee_lua USES_TERMINAL VERBATIM)
  defold_register_sequential_test_command(test_debugger_dap_lua
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee_lua>"
    DEPENDS dap_debuggee_lua)
endif()

defold_add_library(debugger_release_check STATIC ../dap.cpp ../debugger.cpp ../debugger_values.cpp ../debugger_inspect.cpp ../debugger_extension.cpp)
target_compile_definitions(debugger_release_check PRIVATE DM_RELEASE=1)
set_target_properties(debugger_release_check PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_dependencies(build_tests debugger_release_check)
add_custom_target(test_debugger_release
  COMMAND "${CMAKE_COMMAND}" "-DARCHIVE=$<TARGET_FILE:debugger_release_check>" -P "${CMAKE_CURRENT_LIST_DIR}/verify_release.cmake"
  DEPENDS debugger_release_check VERBATIM)
defold_register_sequential_test_command(test_debugger_release
  COMMAND "${CMAKE_COMMAND}" "-DARCHIVE=$<TARGET_FILE:debugger_release_check>" -P "${CMAKE_CURRENT_LIST_DIR}/verify_release.cmake"
  DEPENDS debugger_release_check)

if(TARGET script AND TARGET extension)
  defold_add_executable(dap_debuggee_engine test_debugger_engine.cpp)
  target_compile_definitions(dap_debuggee_engine PRIVATE DLIB_LOG_DOMAIN="DEBUGGER_TEST")
  target_link_libraries(dap_debuggee_engine PRIVATE debugger)
  defold_target_link_libraries(dap_debuggee_engine "${TARGET_PLATFORM}"
    script resource extension graphics_null platform_null ddf dlib profile_null image)
  defold_register_test_target(dap_debuggee_engine OFF)
  add_custom_target(test_debugger_dap_engine
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee_engine>"
    DEPENDS dap_debuggee_engine USES_TERMINAL VERBATIM)
  defold_register_sequential_test_command(test_debugger_dap_engine
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_dap.py" --debuggee "$<TARGET_FILE:dap_debuggee_engine>"
    DEPENDS dap_debuggee_engine)
endif()
