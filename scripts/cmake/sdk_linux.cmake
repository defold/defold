defold_log("sdk_linux.cmake:")

# Use the currently installed clang/clang++ from PATH

# Respect pre-set compilers if user provided them
if(NOT CMAKE_CXX_COMPILER)
  find_program(_DEFOLD_CLANGXX NAMES clang++ REQUIRED)
  set(CMAKE_CXX_COMPILER "${_DEFOLD_CLANGXX}" CACHE FILEPATH "System clang++" FORCE)
endif()

if(NOT CMAKE_C_COMPILER)
  find_program(_DEFOLD_CLANG NAMES clang REQUIRED)
  set(CMAKE_C_COMPILER "${_DEFOLD_CLANG}" CACHE FILEPATH "System clang" FORCE)
endif()

defold_log("sdk_linux: CXX=${CMAKE_CXX_COMPILER}")
defold_log("sdk_linux: CC=${CMAKE_C_COMPILER}")
