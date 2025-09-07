message("defold.cmake:")

get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(DEFOLD_SDK_ROOT "$ENV{DYNAMO_HOME}" CACHE PATH "Path to Defold SDK (e.g. defold/tmp/dynamo_home)")
if(NOT DEFOLD_SDK_ROOT)
  if(EXISTS "${DEFOLD_HOME}/tmp/dynamo_home")
    set(DEFOLD_SDK_ROOT "${DEFOLD_HOME}/tmp/dynamo_home" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)" FORCE)
  endif()
endif()

# if(NOT DEFOLD_SDK_ROOT)
#   message(FATAL_ERROR "DEFOLD_SDK_ROOT not set. Run './scripts/build.py shell && ./scripts/build.py install_ext && ./scripts/build.py build_engine' first, or pass -DDEFOLD_SDK_ROOT=/path/to/tmp/dynamo_home")
# endif()

message(STATUS "DEFOLD_HOME: ${DEFOLD_HOME}")
message(STATUS "DEFOLD_SDK_ROOT: ${DEFOLD_SDK_ROOT}")

# list of toggleable features
include(features)

# platform specific includes, lib paths, defines etc...
include(platform)

# Common paths
set(DEFOLD_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/include")
set(DEFOLD_EXT_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include")
set(DEFOLD_EXT_PLATFORM_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include/${TARGET_PLATFORM}")
set(DEFOLD_DMSDK_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/sdk/include")

set(DEFOLD_LIB_DIR "${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}")
set(DEFOLD_EXT_LIB_DIR "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}")

if(NOT EXISTS "${DEFOLD_SDK_ROOT}")
  message(FATAL_ERROR "Missing folder ${DEFOLD_SDK_ROOT}")
endif()

# Add common paths
include_directories(
    "${DEFOLD_INCLUDE_DIR}"                 # for defold includes (dlib, graphics etc)
    "${DEFOLD_EXT_INCLUDE_DIR}"             # for 3rd party headers
    "${DEFOLD_EXT_PLATFORM_INCLUDE_DIR}"    # for 3rd party, platform sprcific headers
    "${DEFOLD_DMSDK_INCLUDE_DIR}"           # for dmsdk public headers
)
link_directories(
    "${DEFOLD_LIB_DIR}"
    "${DEFOLD_EXT_LIB_DIR}"
)

# Install into DEFOLD_SDK_ROOT
set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}" CACHE PATH "Install prefix" FORCE)
message(STATUS "Install prefix set to DEFOLD_SDK_ROOT: ${CMAKE_INSTALL_PREFIX}")
