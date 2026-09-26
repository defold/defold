cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(lz4 VERSION 1.10.0 LANGUAGES C)

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)

add_library(lz4 STATIC src/lz4.c src/lz4hc.c)
target_link_libraries(lz4 PRIVATE defold_sdk)
set_target_properties(lz4 PROPERTIES PREFIX "lib")

install(TARGETS lz4 ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES src/lz4.h src/lz4hc.h DESTINATION include/lz4)
install(FILES LICENSE DESTINATION share/licenses/lz4)
