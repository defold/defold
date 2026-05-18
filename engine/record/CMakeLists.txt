cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_record LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="RECORD")

set(RECORD_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RECORD_SRC_DIR "${RECORD_ROOT}/src/record")

set(RECORD_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86-win32|x86_64-macos|arm64-macos)$")
  set(RECORD_IS_DESKTOP ON)
endif()

set(RECORD_INSTALL_TARGETS record_null)

defold_add_library(record_null STATIC
  "${RECORD_SRC_DIR}/record_null.cpp"
  "${RECORD_SRC_DIR}/record_common.cpp")
target_include_directories(record_null PUBLIC "${RECORD_SRC_DIR}" "${RECORD_ROOT}/src")
target_compile_definitions(record_null PRIVATE "DLIB_LOG_DOMAIN=\"RECORD\"")
defold_target_link_libraries(record_null "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

defold_feature_disabled(record RECORD_FEATURE_DISABLED)
if(RECORD_IS_DESKTOP AND NOT RECORD_FEATURE_DISABLED)
  defold_add_library(record STATIC
    "${RECORD_SRC_DIR}/record.cpp"
    "${RECORD_SRC_DIR}/record_common.cpp")
  target_include_directories(record PUBLIC "${RECORD_SRC_DIR}" "${RECORD_ROOT}/src")
  target_compile_definitions(record PRIVATE "DLIB_LOG_DOMAIN=\"RECORD\"")
  defold_target_link_libraries(record "${TARGET_PLATFORM}" SCOPE PUBLIC dlib vpx)
  list(APPEND RECORD_INSTALL_TARGETS record)
endif()

defold_install_targets(TARGETS ${RECORD_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES "${RECORD_SRC_DIR}/record.h" DESTINATION include/record)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
