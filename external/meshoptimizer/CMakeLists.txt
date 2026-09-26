cmake_minimum_required(VERSION 4.0)

set(MESHOPT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/meshoptimizer-1.2")
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${MESHOPT_SOURCE_DIR}" upstream)
set_target_properties(meshoptimizer PROPERTIES CXX_STANDARD 98 POSITION_INDEPENDENT_CODE ON)

defold_install_targets(TARGETS meshoptimizer ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES "${MESHOPT_SOURCE_DIR}/src/meshoptimizer.h" DESTINATION include/meshoptimizer)
install(FILES "${MESHOPT_SOURCE_DIR}/LICENSE.md" DESTINATION share/licenses/meshoptimizer)
