cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_liveupdate LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="LIVEUPDATE")

set(LIVEUPDATE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(LIVEUPDATE_SRC_DIR "${LIVEUPDATE_ROOT}/src")

file(GLOB LIVEUPDATE_SOURCES CONFIGURE_DEPENDS "${LIVEUPDATE_SRC_DIR}/*.cpp")
list(FILTER LIVEUPDATE_SOURCES EXCLUDE REGEX "_null\\.cpp$")

defold_add_library(liveupdate STATIC ${LIVEUPDATE_SOURCES})
target_include_directories(liveupdate PUBLIC "${LIVEUPDATE_SRC_DIR}")
target_compile_definitions(liveupdate PRIVATE "DLIB_LOG_DOMAIN=\"LIVEUPDATE\"")
defold_target_link_libraries(liveupdate "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  image
  zip
  mbedtls
  resource
  script
  extension
  lua)

defold_add_library(liveupdate_null STATIC "${LIVEUPDATE_SRC_DIR}/liveupdate_null.cpp")
target_include_directories(liveupdate_null PUBLIC "${LIVEUPDATE_SRC_DIR}")
target_compile_definitions(liveupdate_null PRIVATE "DLIB_LOG_DOMAIN=\"LIVEUPDATE\"")
defold_target_link_libraries(liveupdate_null "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

install(TARGETS liveupdate liveupdate_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES "${LIVEUPDATE_SRC_DIR}/liveupdate.h" DESTINATION include/liveupdate)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
