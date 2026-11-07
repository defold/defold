defold_add_executable(test_data
  "${CMAKE_CURRENT_LIST_DIR}/test_data.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_rows.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_groups.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_scheduler.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_pages.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_memory.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_lifetime.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_view.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_c.c")
target_compile_features(test_data PRIVATE c_std_11)
target_link_libraries(test_data PRIVATE datastore)
defold_target_link_libraries(test_data "${TARGET_PLATFORM}" testmain dlib profile_null)

# Keep the public header compatible with consumers using C++14.
defold_add_library(test_data_cpp14 OBJECT "${CMAKE_CURRENT_LIST_DIR}/test_data_cpp14.cpp")
target_link_libraries(test_data_cpp14 PRIVATE datastore)
set_target_properties(test_data_cpp14 PROPERTIES
  CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(test_data_cpp14 PRIVATE -pedantic-errors)
endif()
add_dependencies(test_data test_data_cpp14)
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
