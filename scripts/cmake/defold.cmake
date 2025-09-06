message("defold.cmake:")


set(DEFOLD_SDK_ROOT "$ENV{DYNAMO_HOME}" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)")
if(NOT DEFOLD_SDK_ROOT)
  set()
  # Fallback to local checkout build output if present
  get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
  if(EXISTS "${_repo_root}/tmp/dynamo_home/include")
    set(DEFOLD_SDK_ROOT "${_repo_root}/tmp/dynamo_home" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)" FORCE)
  endif()
endif()

if(NOT DEFOLD_SDK_ROOT)
  message(FATAL_ERROR "DEFOLD_SDK_ROOT not set. Run './scripts/build.py shell && ./scripts/build.py install_ext && ./scripts/build.py build_engine' first, or pass -DDEFOLD_SDK_ROOT=/path/to/tmp/dynamo_home")
endif()


# set(DEFOLD_ROOT "$ENV{DEFOLD_HOME}" CACHE PATH "Path to Defold repo")
# if(NOT DEFOLD_ROOT)
#   # Fallback to local checkout build output if present
#   get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
#   if(EXISTS "${_repo_root}")
#     set(DEFOLD_ROOT "${_repo_root}" CACHE PATH "Path to Defold repo" FORCE)
#   endif()
# endif()


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
    "${DEFOLD_INCLUDE_DIR}"     # for platform/, dlib/, etc
    "${DEFOLD_EXT_INCLUDE_DIR}" # for ext headers (e.g. jc_test)
    "${DEFOLD_EXT_PLATFORM_INCLUDE_DIR}" # for ext headers (e.g. jc_test)
    "${DEFOLD_DMSDK_INCLUDE_DIR}" # for dmsdk public headers
)
link_directories(
    "${DEFOLD_LIB_DIR}"
    "${DEFOLD_EXT_LIB_DIR}"
)

