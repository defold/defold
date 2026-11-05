cmake_minimum_required(VERSION 4.0)
project(defold_lz4_benchmark LANGUAGES C CXX)

# Use a complete upstream checkout so the frame experiment stays out of the engine.
set(LZ4_SOURCE_DIR "" CACHE PATH "Directory containing upstream lib/lz4.c and lib/lz4frame.c")
if(NOT EXISTS "${LZ4_SOURCE_DIR}/lz4frame.c")
  message(FATAL_ERROR "Set LZ4_SOURCE_DIR to the lib directory of an LZ4 release")
endif()

get_filename_component(DEFOLD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/lz4")
configure_file("${LZ4_SOURCE_DIR}/lz4.h" "include/lz4/lz4.h" COPYONLY)
configure_file("${LZ4_SOURCE_DIR}/lz4hc.h" "include/lz4/lz4hc.h" COPYONLY)

add_executable(lz4_benchmark
  benchmark.cpp
  "${DEFOLD_ROOT}/engine/dlib/src/dlib/lz4.cpp"
  "${LZ4_SOURCE_DIR}/lz4.c"
  "${LZ4_SOURCE_DIR}/lz4hc.c"
  "${LZ4_SOURCE_DIR}/lz4frame.c"
  "${LZ4_SOURCE_DIR}/xxhash.c")
target_include_directories(lz4_benchmark PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${DEFOLD_ROOT}/engine/dlib/src"
  "${LZ4_SOURCE_DIR}")
target_compile_definitions(lz4_benchmark PRIVATE LZ4F_MULTITHREAD=1)
target_link_libraries(lz4_benchmark PRIVATE Threads::Threads)
