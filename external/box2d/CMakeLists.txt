cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(box2d_external VERSION 3.2.0 LANGUAGES C)

file(GLOB BOX2D_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/Box2D/src/*.c")

foreach(_box2d_target IN ITEMS box2d box2d_nosimd)
  add_library(${_box2d_target} STATIC ${BOX2D_SOURCES})
  target_include_directories(${_box2d_target} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Box2D/include"
    "${CMAKE_CURRENT_LIST_DIR}/Box2D/src")
  target_compile_features(${_box2d_target} PRIVATE c_std_17)
  target_link_libraries(${_box2d_target} PRIVATE defold_sdk)
  # Match upstream's deterministic math across scalar, SSE2, AVX2 and NEON.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      target_compile_options(${_box2d_target} PRIVATE /clang:-ffp-contract=off)
    else()
      target_compile_options(${_box2d_target} PRIVATE -ffp-contract=off)
    endif()
  endif()
endforeach()

target_compile_definitions(box2d_nosimd PRIVATE BOX2D_DISABLE_SIMD)

if(MSVC AND CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND TARGET_PLATFORM STREQUAL "x86_64-win32")
  set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Box2D/src/contact_solver_w8.c"
    PROPERTIES COMPILE_OPTIONS /arch:AVX2)
endif()

if(TARGET_PLATFORM MATCHES "^(wasm|wasm_pthread)-web$")
  target_compile_options(box2d PRIVATE -matomics -mbulk-memory -msimd128 -msse2)
endif()

if(TARGET_PLATFORM MATCHES "win32$")
  set_target_properties(box2d box2d_nosimd PROPERTIES PREFIX "lib")
endif()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS box2d box2d_nosimd ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Box2D/include/box2d"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Box2D/src/"
        DESTINATION include/box2d/src
        FILES_MATCHING PATTERN "*.h" PATTERN "*.inl")
