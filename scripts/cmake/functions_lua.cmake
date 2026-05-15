defold_log("functions_lua.cmake:")

include(functions_libs)

# Link the correct Lua implementation for the given platform.
# - Non-web platforms: link LuaJIT (luajit-5.1)
# - Web platforms: link the vanilla lua library.
#
# Usage:
#   defold_target_link_lua(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])

function(defold_target_link_lua target platform)
  set(options)
  set(oneValueArgs SCOPE)
  set(multiValueArgs)
  cmake_parse_arguments(DLUA "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DLUA_SCOPE)
    set(DLUA_SCOPE PRIVATE)
  endif()

  if(NOT target OR NOT platform)
    message(FATAL_ERROR "defold_target_link_lua: target and platform are required")
  endif()

  if("${platform}" MATCHES "^(js-web|wasm-web|wasm_pthread-web)$")
    defold_target_link_libraries(${target} ${platform} SCOPE ${DLUA_SCOPE} lua)
  else()
    defold_target_link_libraries(${target} ${platform} SCOPE ${DLUA_SCOPE} luajit-5.1)
  endif()
endfunction()
