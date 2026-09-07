cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../scripts/cmake")
include(defold)

project(defold_ext LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)

add_subdirectory(bullet3d)
