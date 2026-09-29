cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_data LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(DATA_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DATA_SRC_DIR "${DATA_ROOT}/src")
set(DATA_DMSDK_HEADERS
  "${DATA_SRC_DIR}/dmsdk/data/data.h"
  "${DATA_SRC_DIR}/dmsdk/data/data_types.h"
  "${DATA_SRC_DIR}/dmsdk/data/data_query.h"
  "${DATA_SRC_DIR}/dmsdk/data/data_iter.h"
  "${DATA_SRC_DIR}/dmsdk/data/data_field.h")

defold_add_library(data STATIC
  "${DATA_SRC_DIR}/data.cpp"
  "${DATA_SRC_DIR}/data_query.cpp"
  "${DATA_SRC_DIR}/data_iter.cpp"
  "${DATA_SRC_DIR}/data_field.cpp"
  "${DATA_SRC_DIR}/data_io.cpp")
target_include_directories(data PUBLIC "${DATA_SRC_DIR}")
defold_target_link_libraries(data "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

defold_install_targets(TARGETS data
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION bin/${TARGET_PLATFORM})
defold_add_sdk_headers_target(data_public_sdk_headers
  DESTINATION include/data
  FILES "${DATA_SRC_DIR}/data.h"
  DEPENDS data)
defold_add_sdk_headers_target(data_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk/data
  FILES ${DATA_DMSDK_HEADERS}
  DEPENDS data)
add_custom_target(data_sdk_headers
  DEPENDS data_public_sdk_headers data_dmsdk_sdk_headers)
install(FILES "${DATA_SRC_DIR}/data.h" DESTINATION include/data)
install(FILES ${DATA_DMSDK_HEADERS} DESTINATION sdk/include/dmsdk/data)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
