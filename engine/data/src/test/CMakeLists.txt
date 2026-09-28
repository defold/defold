defold_add_executable(test_data
  "${CMAKE_CURRENT_LIST_DIR}/test_data.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_c.c")
target_link_libraries(test_data PRIVATE data)
defold_target_link_libraries(test_data "${TARGET_PLATFORM}" testmain dlib profile_null)
defold_register_test_target(test_data ON "${DATA_ROOT}")
