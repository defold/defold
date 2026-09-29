defold_add_executable(test_data
  "${CMAKE_CURRENT_LIST_DIR}/test_data.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test_data_c.c")
target_link_libraries(test_data PRIVATE data)
defold_target_link_libraries(test_data "${TARGET_PLATFORM}" testmain dlib profile_null)
defold_register_test_target(test_data ON "${DATA_ROOT}")

if(WITH_TSAN)
  if(NOT TARGET dlib)
    message(FATAL_ERROR "Threading tests require a source-built, TSAN-instrumented dlib. Configure from the repository root with DEFOLD_SELECTED_ENGINE_LIBS=dlib,testmain,data.")
  endif()
  defold_add_executable(test_data_threaded "${CMAKE_CURRENT_LIST_DIR}/test_data_threaded.cpp")
  target_link_libraries(test_data_threaded PRIVATE data)
  defold_target_link_libraries(test_data_threaded "${TARGET_PLATFORM}" testmain dlib profile_null)
  defold_register_test_target(test_data_threaded ON "${DATA_ROOT}")
endif()

# Flecs is a benchmark input, never a runtime dependency or an automatic download.
set(DEFOLD_DATA_FLECS_DIR "" CACHE PATH "Flecs checkout containing distr/flecs.c and distr/flecs.h")
if(DEFOLD_DATA_FLECS_DIR)
  if(NOT EXISTS "${DEFOLD_DATA_FLECS_DIR}/distr/flecs.c" OR NOT EXISTS "${DEFOLD_DATA_FLECS_DIR}/distr/flecs.h")
    message(FATAL_ERROR "DEFOLD_DATA_FLECS_DIR must contain distr/flecs.c and distr/flecs.h")
  endif()
  add_library(data_benchmark_flecs STATIC EXCLUDE_FROM_ALL "${DEFOLD_DATA_FLECS_DIR}/distr/flecs.c")
  target_include_directories(data_benchmark_flecs PUBLIC "${DEFOLD_DATA_FLECS_DIR}/distr")
  target_compile_definitions(data_benchmark_flecs PUBLIC FLECS_NO_CPP FLECS_NDEBUG)
  if(WIN32)
    target_link_libraries(data_benchmark_flecs PUBLIC ws2_32 dbghelp)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(data_benchmark_flecs PUBLIC Threads::Threads m)
  endif()
  defold_add_executable(benchmark_data "${CMAKE_CURRENT_LIST_DIR}/benchmark_data.cpp")
  set_target_properties(benchmark_data PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_link_libraries(benchmark_data PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data "${TARGET_PLATFORM}" dlib profile_null)

  set(DATA_MIXED_BENCHMARK_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_mixed.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_common.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_create.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_add.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_churn.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_light_color.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_health_position.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_explosion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_shuffled_position.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_packed.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_packed_access.cpp")
  defold_add_executable(benchmark_data_mixed ${DATA_MIXED_BENCHMARK_SOURCES})
  set_target_properties(benchmark_data_mixed PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_link_libraries(benchmark_data_mixed PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data_mixed "${TARGET_PLATFORM}" dlib profile_null)
  # Separate executable: allocation tracking must not affect timing measurements.
  defold_add_executable(benchmark_data_memory
    ${DATA_MIXED_BENCHMARK_SOURCES}
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_memory.cpp")
  set_target_properties(benchmark_data_memory PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_compile_definitions(benchmark_data_memory PRIVATE DATA_BENCHMARK_MEMORY)
  target_link_libraries(benchmark_data_memory PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data_memory "${TARGET_PLATFORM}" dlib profile_null)

  defold_add_executable(benchmark_data_blob_load
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_blob_load.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_memory.cpp")
  set_target_properties(benchmark_data_blob_load PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_compile_definitions(benchmark_data_blob_load PRIVATE DATA_BENCHMARK_MEMORY)
  target_link_libraries(benchmark_data_blob_load PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data_blob_load "${TARGET_PLATFORM}" dlib profile_null)
endif()
