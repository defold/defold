defold_log("functions_libs.cmake:")

# Static link closure for protobuf 34.0's protobuf::libprotobuf target. Keep
# this in the order reported by protobuf's pkg-config metadata.
set(DEFOLD_PROTOBUF_LIBS
  protobuf
  absl_log_internal_check_op absl_die_if_null absl_log_internal_conditions
  absl_log_internal_message absl_examine_stack absl_log_internal_format
  absl_log_internal_nullguard absl_log_internal_structured_proto
  absl_log_internal_proto absl_log_internal_log_sink_set absl_log_sink
  absl_flags_internal absl_flags_marshalling absl_flags_reflection
  absl_flags_private_handle_accessor absl_flags_commandlineflag
  absl_flags_commandlineflag_internal absl_flags_config absl_flags_program_name
  absl_log_initialize absl_log_internal_globals absl_log_globals
  absl_vlog_config_internal absl_log_internal_fnmatch absl_raw_hash_set
  absl_hash absl_city absl_low_level_hash absl_hashtablez_sampler
  absl_random_distributions absl_random_seed_sequences
  absl_random_internal_entropy_pool absl_random_internal_randen
  absl_random_internal_randen_hwaes absl_random_internal_randen_hwaes_impl
  absl_random_internal_randen_slow absl_random_internal_platform
  absl_random_internal_seed_material absl_random_seed_gen_exception
  absl_statusor absl_status absl_cord absl_cordz_info absl_cord_internal
  absl_cordz_functions absl_exponential_biased absl_cordz_handle
  absl_crc_cord_state absl_crc32c absl_crc_internal absl_crc_cpu_detect
  absl_leak_check absl_strerror absl_str_format_internal absl_synchronization
  absl_graphcycles_internal absl_kernel_timeout_internal absl_stacktrace
  absl_symbolize absl_debugging_internal absl_demangle_internal
  absl_demangle_rust absl_decode_rust_punycode absl_utf8_for_code_point
  absl_malloc_internal absl_tracing_internal absl_time absl_civil_time
  absl_time_zone utf8_validity utf8_range absl_strings absl_strings_internal
  absl_string_view absl_int128 absl_base absl_spinlock_wait
  absl_throw_delegate absl_raw_logging_internal absl_log_severity
)

# defold_target_link_libraries
# Link libraries to a target with platform-aware name adjustments.
#
# Usage:
#   defold_target_link_libraries(<target> <platform>
#                                 [SCOPE <PRIVATE|PUBLIC|INTERFACE>] <libs...>)
#
# Behavior:
# - For Windows platforms (…-win32), Defold static libraries are prefixed with
#   "lib" unless they already use an exact library name. Protobuf's transitive
#   Abseil libraries keep their upstream Windows names.
# - Other exceptions that already follow Windows naming: hid, hid_null, input,
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
  set(_LIBS)
  foreach(_lib IN LISTS DLIB_UNPARSED_ARGUMENTS)
    if(_lib STREQUAL "PROTOBUF")
      list(APPEND _LIBS ${DEFOLD_PROTOBUF_LIBS})
      if(platform MATCHES "-android$")
        list(APPEND _LIBS c++_static c++abi)
      endif()
    else()
      list(APPEND _LIBS "${_lib}")
    endif()
  endforeach()

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
         OR _lib STREQUAL "platform" OR _lib STREQUAL "platform_null" OR _lib STREQUAL "platform_vulkan"
         OR _lib MATCHES "^absl_")
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
  if(NOT "${platform}" MATCHES "^(wasm-web|wasm_pthread-web)$")
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
