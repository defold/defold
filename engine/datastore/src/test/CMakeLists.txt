defold_add_executable(test_data
  "${CMAKE_CURRENT_LIST_DIR}/test_data.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_rows.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_memory.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_lifetime.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_view.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_c.c")
target_link_libraries(test_data PRIVATE datastore)
defold_target_link_libraries(test_data "${TARGET_PLATFORM}" testmain dlib profile_null)
defold_register_test_target(test_data ON "${DATA_ROOT}")

if(WITH_TSAN)
  if(NOT TARGET dlib)
    message(FATAL_ERROR "Threading tests require a source-built, TSAN-instrumented dlib. Configure from the repository root with DEFOLD_SELECTED_ENGINE_LIBS=dlib,testmain,datastore.")
  endif()
  defold_add_executable(test_data_threaded "${CMAKE_CURRENT_LIST_DIR}/test_data_threaded.cpp")
  target_link_libraries(test_data_threaded PRIVATE datastore)
  defold_target_link_libraries(test_data_threaded "${TARGET_PLATFORM}" testmain dlib profile_null)
  defold_register_test_target(test_data_threaded ON "${DATA_ROOT}")
endif()

set_source_files_properties(test_data_c.c PROPERTIES COMPILE_DEFINITIONS DATA_PUBLIC_API_ONLY)
