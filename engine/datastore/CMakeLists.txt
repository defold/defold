cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_datastore LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(DATA_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DATA_SRC_DIR "${DATA_ROOT}/src")
set(DATA_DMSDK_HEADERS
  "${DATA_SRC_DIR}/dmsdk/datastore/datastore.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_types.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_query.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_iter.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_field.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_container.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_table.h"
  "${DATA_SRC_DIR}/dmsdk/datastore/data_blob.h")

defold_add_library(datastore STATIC
  "${DATA_SRC_DIR}/data.cpp"
  "${DATA_SRC_DIR}/data_query.cpp"
  "${DATA_SRC_DIR}/data_iter.cpp"
  "${DATA_SRC_DIR}/data_field.cpp"
  "${DATA_SRC_DIR}/data_container.cpp"
  "${DATA_SRC_DIR}/data_struct.cpp"
  "${DATA_SRC_DIR}/data_payload.cpp"
  "${DATA_SRC_DIR}/data_io.cpp")
target_include_directories(datastore PUBLIC "${DATA_SRC_DIR}")
defold_target_link_libraries(datastore "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

defold_install_targets(TARGETS datastore
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION bin/${TARGET_PLATFORM})
defold_add_sdk_headers_target(datastore_public_sdk_headers
  DESTINATION include/datastore
  FILES "${DATA_SRC_DIR}/data.h"
  DEPENDS datastore)
defold_add_sdk_headers_target(datastore_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk/datastore
  FILES ${DATA_DMSDK_HEADERS}
  DEPENDS datastore)
add_custom_target(datastore_sdk_headers
  DEPENDS datastore_public_sdk_headers datastore_dmsdk_sdk_headers)
install(FILES "${DATA_SRC_DIR}/data.h" DESTINATION include/datastore)
install(FILES ${DATA_DMSDK_HEADERS} DESTINATION sdk/include/dmsdk/datastore)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
