defold_log("functions_libs.cmake:")

# defold_target_link_libraries
# Link libraries to a target with platform-aware name adjustments.
#
# Usage:
#   defold_target_link_libraries(<target> <platform>
#                                 [SCOPE <PRIVATE|PUBLIC|INTERFACE>] <libs...>)
#
# Behavior:
# - For Windows platforms (…-win32), each library name in <libs> is prefixed
#   with "lib" unless it already starts with "lib", is an absolute path,
#   is a generator expression (starts with "$<"), is a linker flag (starts with "-"),
#   or already ends with ".lib".
# - Exceptions (these already follow Windows naming): hid, hid_null, input,
#   platform, platform_null, platform_vulkan.
# - Other platforms link the names as-is.

function(defold_target_link_libraries target platform)
  set(options)
  set(oneValueArgs SCOPE)
  set(multiValueArgs)
  cmake_parse_arguments(DLIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DLIB_SCOPE)
    set(DLIB_SCOPE PRIVATE)
  endif()

  if(NOT target OR NOT platform)
    message(FATAL_ERROR "defold_target_link_libraries: target and platform are required")
  endif()

  # Remaining unparsed arguments are libraries to link
  set(_LIBS ${DLIB_UNPARSED_ARGUMENTS})

  # Derive OS from tuple (e.g., x86_64-win32 -> win32)
  string(REGEX REPLACE "^[^-]+-" "" _PLAT_OS "${platform}")

  set(_MAPPED_LIBS)
  if(_PLAT_OS STREQUAL "win32")
    foreach(_lib IN LISTS _LIBS)
      set(_mapped "${_lib}")
      # Exceptions: these libs follow Windows naming (no implicit "lib" prefix)
      set(_is_exception OFF)
      if(_lib STREQUAL "hid" OR _lib STREQUAL "hid_null"
         OR _lib STREQUAL "input"
         OR _lib STREQUAL "platform" OR _lib STREQUAL "platform_null" OR _lib STREQUAL "platform_vulkan")
        set(_is_exception ON)
      endif()

      if(NOT _is_exception
         AND NOT IS_ABSOLUTE "${_lib}"
         AND NOT _lib MATCHES "^\\$<"
         AND NOT _lib MATCHES "^-"
         AND NOT _lib MATCHES "^lib"
         AND NOT _lib MATCHES "\\.lib$")
        set(_mapped "lib${_lib}")
      endif()
      list(APPEND _MAPPED_LIBS "${_mapped}")
    endforeach()
  else()
    set(_MAPPED_LIBS ${_LIBS})
  endif()

  if(_MAPPED_LIBS)
    target_link_libraries(${target} ${DLIB_SCOPE} ${_MAPPED_LIBS})
  endif()
endfunction()

# Attach a local include folder to a target and add headers to the IDE tree.
# Usage:
#   defold_attach_local_include(<target> [DIR <path>])
function(defold_attach_local_include target)
  set(options)
  set(oneValueArgs DIR)
  set(multiValueArgs)
  cmake_parse_arguments(DINC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT TARGET ${target})
    return()
  endif()

  set(_inc_dir "")
  if(DINC_DIR)
    set(_inc_dir "${DINC_DIR}")
  else()
    # Conventional local include folder next to current list file
    set(_candidate "${CMAKE_CURRENT_LIST_DIR}/include")
    if(EXISTS "${_candidate}" AND IS_DIRECTORY "${_candidate}")
      set(_inc_dir "${_candidate}")
    endif()
  endif()

  if(NOT _inc_dir)
    return()
  endif()

  # Add include directory for compilation
  target_include_directories(${target} PRIVATE "${_inc_dir}")

  # Add headers to the target's sources so IDEs show them.
  file(GLOB_RECURSE _headers CONFIGURE_DEPENDS
       "${_inc_dir}/*.h" "${_inc_dir}/*.hpp" "${_inc_dir}/*.hh" "${_inc_dir}/*.hxx" "${_inc_dir}/*.inl" "${_inc_dir}/*.inc")
  if(_headers)
    target_sources(${target} PRIVATE ${_headers})
  endif()
endfunction()

# Wrapper around add_executable that also groups sources by folder in IDEs.
# Usage:
#   defold_add_executable(<name>
#                         [EXCLUDE_FROM_ALL]
#                         source1 [source2 ...])
function(defold_add_executable target)
  if(NOT target)
    message(FATAL_ERROR "defold_add_executable: target name is required")
  endif()

  # Forward all remaining args directly to add_executable
  add_executable(${target} ${ARGN})

  # Attach local include dir (e.g., ./include) and headers if present
  if(COMMAND defold_attach_local_include)
    defold_attach_local_include(${target})
  endif()
endfunction()

# Link Emscripten JS libraries for web platforms only.
# Usage:
#   defold_target_link_libraries_web(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>] <js_libs...>)
function(defold_target_link_libraries_web target platform)
  set(options)
  set(oneValueArgs SCOPE)
  set(multiValueArgs)
  cmake_parse_arguments(DWEB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DWEB_SCOPE)
    set(DWEB_SCOPE PRIVATE)
  endif()

  if(NOT target OR NOT platform)
    message(FATAL_ERROR "defold_target_link_libraries_web: target and platform are required")
  endif()

  # Only for web platforms
  if(NOT "${platform}" MATCHES "^(js-web|wasm-web|wasm_pthread-web)$")
    return()
  endif()

  # Remaining unparsed arguments are js library filenames
  set(_JS_LIBS ${DWEB_UNPARSED_ARGUMENTS})
  if(NOT _JS_LIBS)
    return()
  endif()

  set(_js_dir "${DEFOLD_SDK_ROOT}/lib/${platform}/js")
  foreach(_js IN LISTS _JS_LIBS)
    if(EXISTS "${_js_dir}/${_js}")
      target_link_options(${target} ${DWEB_SCOPE} "SHELL:--js-library=${_js_dir}/${_js}")
    else()
      defold_log("functions_libs: JS lib not found: ${_js_dir}/${_js}")
    endif()
  endforeach()
endfunction()

# Wrapper around add_library that also groups sources by folder in IDEs.
# Usage:
#   defold_add_library(<name> [STATIC|SHARED|OBJECT|MODULE|INTERFACE]
#                      [EXCLUDE_FROM_ALL]
#                      source1 [source2 ...])
function(defold_add_library target)
  if(NOT target)
    message(FATAL_ERROR "defold_add_library: target name is required")
  endif()

  # Forward all remaining args directly to add_library
  add_library(${target} ${ARGN})

  # Attach local include dir (e.g., ./include) and headers if present
  if(COMMAND defold_attach_local_include)
    defold_attach_local_include(${target})
  endif()
endfunction()
