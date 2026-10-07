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

# Caller-owned jobs exercise the data library without requiring Flecs.
set(DATA_THREADED_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_resources.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_fixture.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_movement.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_explosion.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_regenerate.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_lights.cpp")
defold_add_executable(benchmark_data_threaded ${DATA_THREADED_SOURCES})
set_target_properties(benchmark_data_threaded PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_link_libraries(benchmark_data_threaded PRIVATE data)
defold_target_link_libraries(benchmark_data_threaded "${TARGET_PLATFORM}" dlib profile_null)
defold_add_executable(benchmark_data_threaded_memory
  ${DATA_THREADED_SOURCES}
  "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_memory.cpp")
target_compile_definitions(benchmark_data_threaded_memory PRIVATE DATA_THREADED_MEMORY)
set_target_properties(benchmark_data_threaded_memory PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_link_libraries(benchmark_data_threaded_memory PRIVATE data)
defold_target_link_libraries(benchmark_data_threaded_memory "${TARGET_PLATFORM}" dlib profile_null)

# EnTT is header-only. Its C++20 requirement applies only to these benchmarks.
set(DEFOLD_DATA_ENTT_DIR "" CACHE PATH "EnTT checkout containing src/entt/entity/registry.hpp")
if(DEFOLD_DATA_ENTT_DIR)
  if(NOT EXISTS "${DEFOLD_DATA_ENTT_DIR}/src/entt/entity/registry.hpp")
    message(FATAL_ERROR "DEFOLD_DATA_ENTT_DIR must contain src/entt/entity/registry.hpp")
  endif()
  add_library(data_benchmark_entt INTERFACE)
  target_include_directories(data_benchmark_entt SYSTEM INTERFACE "${DEFOLD_DATA_ENTT_DIR}/src")
  target_compile_features(data_benchmark_entt INTERFACE cxx_std_20)
  target_compile_definitions(data_benchmark_entt INTERFACE ENTT_USE_ATOMIC ENTT_DISABLE_ASSERT)
  set(DATA_ENTT_THREADED_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_entt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_fixture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_movement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_explosion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_regenerate.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_lights.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_entt_movement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_entt_explosion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_entt_regenerate.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_entt_lights.cpp")
  foreach(target benchmark_data_threaded_entt benchmark_data_threaded_entt_memory)
    defold_add_executable(${target} ${DATA_ENTT_THREADED_SOURCES})
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_link_libraries(${target} PRIVATE data data_benchmark_entt)
    defold_target_link_libraries(${target} "${TARGET_PLATFORM}" dlib profile_null)
  endforeach()
  target_sources(benchmark_data_threaded_entt_memory PRIVATE "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_memory.cpp")
  target_compile_definitions(benchmark_data_threaded_entt_memory PRIVATE DATA_THREADED_MEMORY)
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
  # Native query statistics use unsynchronized counters by default. Threaded
  # timing and TSAN builds both disable these optional statistics.
  add_library(data_benchmark_flecs_threaded STATIC EXCLUDE_FROM_ALL "${DEFOLD_DATA_FLECS_DIR}/distr/flecs.c")
  target_include_directories(data_benchmark_flecs_threaded PUBLIC "${DEFOLD_DATA_FLECS_DIR}/distr")
  target_compile_definitions(data_benchmark_flecs_threaded PUBLIC FLECS_NO_CPP FLECS_NDEBUG FLECS_DISABLE_COUNTERS)
  if(WITH_TSAN)
    target_compile_options(data_benchmark_flecs_threaded PRIVATE -fsanitize=thread -g)
    target_link_options(data_benchmark_flecs_threaded INTERFACE -fsanitize=thread)
  endif()
  set(DATA_FLECS_THREADED_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_flecs.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_fixture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_movement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_explosion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_regenerate.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_lights.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_flecs_movement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_flecs_explosion.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_flecs_regenerate.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_flecs_lights.cpp")
  foreach(target benchmark_data_threaded_flecs benchmark_data_threaded_flecs_memory)
    defold_add_executable(${target} ${DATA_FLECS_THREADED_SOURCES})
    target_compile_definitions(${target} PRIVATE DATA_THREADED_FLECS)
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_link_libraries(${target} PRIVATE data data_benchmark_flecs_threaded)
    defold_target_link_libraries(${target} "${TARGET_PLATFORM}" dlib profile_null)
  endforeach()
  target_sources(benchmark_data_threaded_flecs_memory PRIVATE "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_threaded_memory.cpp")
  target_compile_definitions(benchmark_data_threaded_flecs_memory PRIVATE DATA_THREADED_MEMORY)
  if(WIN32)
    target_link_libraries(data_benchmark_flecs PUBLIC ws2_32 dbghelp)
    target_link_libraries(data_benchmark_flecs_threaded PUBLIC ws2_32 dbghelp)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(data_benchmark_flecs PUBLIC Threads::Threads m)
    target_link_libraries(data_benchmark_flecs_threaded PUBLIC Threads::Threads m)
  endif()
  defold_add_executable(benchmark_data "${CMAKE_CURRENT_LIST_DIR}/benchmark_data.cpp")
  set_target_properties(benchmark_data PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_link_libraries(benchmark_data PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data "${TARGET_PLATFORM}" dlib profile_null)

  set(DATA_MIXED_BENCHMARK_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_fixture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_resources.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_mixed.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_common.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_core.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_boids.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_nearby_lights.cpp"
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

  # Keep EnTT's C++20 requirement confined to its own core executables.
  if(DEFOLD_DATA_ENTT_DIR)
    foreach(target benchmark_data_core_entt benchmark_data_core_entt_memory)
      defold_add_executable(${target}
        ${DATA_MIXED_BENCHMARK_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_core_entt.cpp")
      set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
      target_compile_definitions(${target} PRIVATE DATA_BENCHMARK_ENTT)
      target_link_libraries(${target} PRIVATE data data_benchmark_flecs data_benchmark_entt)
      defold_target_link_libraries(${target} "${TARGET_PLATFORM}" dlib profile_null)
    endforeach()
    target_sources(benchmark_data_core_entt_memory PRIVATE "${CMAKE_CURRENT_LIST_DIR}/benchmark_memory.cpp")
    target_compile_definitions(benchmark_data_core_entt_memory PRIVATE DATA_BENCHMARK_MEMORY)
  endif()

  defold_add_executable(benchmark_data_blob_load
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_blob_load.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_memory.cpp")
  set_target_properties(benchmark_data_blob_load PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_compile_definitions(benchmark_data_blob_load PRIVATE DATA_BENCHMARK_MEMORY)
  target_link_libraries(benchmark_data_blob_load PRIVATE data data_benchmark_flecs)
  defold_target_link_libraries(benchmark_data_blob_load "${TARGET_PLATFORM}" dlib profile_null)
endif()

# Small, equivalent linked programs measure header-only EnTT alongside archives.
# Build only in an unsanitized configuration; the runner checks its flags.
if(DEFOLD_DATA_FLECS_DIR AND DEFOLD_DATA_ENTT_DIR)
  foreach(backend empty defold flecs entt)
    defold_add_executable(benchmark_data_size_${backend}
      "${CMAKE_CURRENT_LIST_DIR}/benchmark_data_size_${backend}.cpp")
    set_target_properties(benchmark_data_size_${backend} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    if(APPLE)
      target_link_options(benchmark_data_size_${backend} PRIVATE -Wl,-dead_strip)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_options(benchmark_data_size_${backend} PRIVATE -Wl,--gc-sections)
    endif()
  endforeach()
  target_link_libraries(benchmark_data_size_defold PRIVATE data)
  defold_target_link_libraries(benchmark_data_size_defold "${TARGET_PLATFORM}" dlib profile_null)
  target_link_libraries(benchmark_data_size_flecs PRIVATE data_benchmark_flecs_threaded)
  target_link_libraries(benchmark_data_size_entt PRIVATE data_benchmark_entt)
endif()

# Serialize resources outside every benchmark binary and timed phase.
set(DATA_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
set(DATA_FIXTURE_SOURCES data_fixture_writer.cpp benchmark_data_threaded_fixture.cpp)
if(DEFOLD_DATA_FLECS_DIR)
  list(APPEND DATA_FIXTURE_SOURCES benchmark_data_fixture.cpp)
endif()
defold_add_executable(data_fixture_writer ${DATA_FIXTURE_SOURCES})
set_target_properties(data_fixture_writer PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_link_libraries(data_fixture_writer PRIVATE data)
defold_target_link_libraries(data_fixture_writer "${TARGET_PLATFORM}" dlib profile_null)
if(DEFOLD_DATA_FLECS_DIR)
  target_compile_definitions(data_fixture_writer PRIVATE DATA_FIXTURE_MIXED)
  target_link_libraries(data_fixture_writer PRIVATE data_benchmark_flecs)
endif()
add_custom_command(OUTPUT "${DATA_FIXTURE_DIR}/ready"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DATA_FIXTURE_DIR}"
  COMMAND $<TARGET_FILE:data_fixture_writer> "${DATA_FIXTURE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E touch "${DATA_FIXTURE_DIR}/ready"
  DEPENDS data_fixture_writer VERBATIM)
add_custom_target(data_benchmark_fixtures DEPENDS "${DATA_FIXTURE_DIR}/ready")
foreach(target benchmark_data_mixed benchmark_data_memory benchmark_data_core_entt benchmark_data_core_entt_memory benchmark_data_threaded benchmark_data_threaded_memory benchmark_data_blob_load)
  if(TARGET ${target})
    add_dependencies(${target} data_benchmark_fixtures)
    target_compile_definitions(${target} PRIVATE DATA_BENCHMARK_FIXTURE_DIR="${DATA_FIXTURE_DIR}")
  endif()
endforeach()

# Benchmarks exercise the same data API available to clients. Reject accidental
# private includes, including those introduced through shared fixture headers.
get_property(DATA_TEST_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS DATA_TEST_TARGETS)
  if(target MATCHES "^benchmark_")
    target_compile_definitions(${target} PRIVATE DATA_PUBLIC_API_ONLY)
  endif()
endforeach()
set_source_files_properties(test_data_c.c PROPERTIES COMPILE_DEFINITIONS DATA_PUBLIC_API_ONLY)
