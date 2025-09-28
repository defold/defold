cmake_minimum_required(VERSION 4.0)

# Top-level CMake for Defold components (hid, input)

# Ensure our CMake modules are available
include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/defold.cmake")

# we rename this project in build.py when we generate the solution
project(defold_libraries LANGUAGES C CXX)

# Add subprojects
add_subdirectory(engine/hid)
add_subdirectory(engine/input)
