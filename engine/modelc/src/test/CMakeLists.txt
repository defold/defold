set(MODELC_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}")

defold_add_executable(test_model "${MODELC_TEST_DIR}/test_model.cpp" "${MODELC_TEST_DIR}/test_model_compression.cpp")
target_include_directories(test_model PRIVATE "${MODELC_SRC_DIR}" "${MODELC_TEST_DIR}/..")
defold_target_link_libraries(test_model "${TARGET_PLATFORM}"
  model
  dlib
  zip
  mbedtls
  profile)
defold_register_test_target(test_model ON "${MODELC_ROOT}")

# Explicit benchmark target; it does not add work to normal builds or tests.
defold_add_executable(benchmark_model_buffers EXCLUDE_FROM_ALL "${MODELC_TEST_DIR}/benchmark_model_buffers.cpp")
target_include_directories(benchmark_model_buffers PRIVATE "${MODELC_SRC_DIR}" "${DEFOLD_HOME}/engine/gamesys/src")
defold_target_link_libraries(benchmark_model_buffers "${TARGET_PLATFORM}" model dlib zip mbedtls profile)
