defold_log("functions_csharp.cmake:")

function(defold_dotnet_runtime_identifier out_var platform)
  string(REGEX REPLACE "-.*$" "" _arch "${platform}")
  string(REGEX REPLACE "^[^-]+-" "" _os "${platform}")

  if(_os STREQUAL "macos")
    set(_rid_os "osx")
  elseif(_os STREQUAL "win32")
    set(_rid_os "win")
  else()
    set(_rid_os "${_os}")
  endif()

  if(_arch STREQUAL "x86_64")
    set(_rid_arch "x64")
  elseif(_arch STREQUAL "armv7")
    set(_rid_arch "arm")
  else()
    set(_rid_arch "${_arch}")
  endif()

  set(${out_var} "${_rid_os}-${_rid_arch}" PARENT_SCOPE)
endfunction()

function(defold_find_dotnet out_var)
  find_program(_dotnet dotnet)
  set(${out_var} "${_dotnet}" PARENT_SCOPE)
endfunction()

function(defold_add_csharp_static_library target)
  set(options)
  set(oneValueArgs PROJECT OUTPUT_VAR)
  set(multiValueArgs)
  cmake_parse_arguments(DCS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DCS_PROJECT)
    message(FATAL_ERROR "defold_add_csharp_static_library: PROJECT is required")
  endif()

  defold_find_dotnet(_dotnet)
  if(NOT _dotnet)
    message(STATUS "dotnet not found; skipping C# target ${target}")
    if(DCS_OUTPUT_VAR)
      set(${DCS_OUTPUT_VAR} "" PARENT_SCOPE)
    endif()
    return()
  endif()

  defold_dotnet_runtime_identifier(_rid "${TARGET_PLATFORM}")

  get_filename_component(_project_abs "${DCS_PROJECT}" ABSOLUTE)
  get_filename_component(_project_dir "${_project_abs}" DIRECTORY)
  get_filename_component(_project_name "${_project_abs}" NAME_WE)

  if(TARGET_PLATFORM MATCHES "-win32$")
    set(_lib_prefix "")
    set(_lib_suffix ".lib")
  else()
    set(_lib_prefix "lib")
    set(_lib_suffix ".a")
  endif()

  set(_build_dir "${CMAKE_CURRENT_BINARY_DIR}/cs/${target}")
  set(_output_lib "${_build_dir}/${_lib_prefix}${_project_name}${_lib_suffix}")

  file(GLOB_RECURSE _cs_sources CONFIGURE_DEPENDS
       "${_project_dir}/*.cs"
       "${_project_dir}/../../cs/*.cs")

  add_custom_command(
    OUTPUT "${_output_lib}"
    COMMAND "${_dotnet}" publish -c Release -o "${_build_dir}" -v q
            --artifacts-path "${_build_dir}"
            -r "${_rid}"
            -p:PublishAot=true
            -p:NativeLib=Static
            -p:PublishTrimmed=true
            -p:IlcDehydrate=false
            "${_project_abs}"
    DEPENDS "${_project_abs}" ${_cs_sources}
    WORKING_DIRECTORY "${_project_dir}"
    VERBATIM
    COMMENT "Publishing C# NativeAOT static library ${target}")

  add_custom_target(${target}_build DEPENDS "${_output_lib}")
  add_library(${target} STATIC IMPORTED GLOBAL)
  set_target_properties(${target} PROPERTIES IMPORTED_LOCATION "${_output_lib}")
  add_dependencies(${target} ${target}_build)

  if(DCS_OUTPUT_VAR)
    set(${DCS_OUTPUT_VAR} "${_output_lib}" PARENT_SCOPE)
  endif()
endfunction()

function(defold_csharp_nativeaot_runtime_available out_var platform)
  set(${out_var} FALSE PARENT_SCOPE)

  if(NOT platform MATCHES "-macos$")
    return()
  endif()

  defold_find_dotnet(_dotnet)
  if(NOT _dotnet)
    return()
  endif()

  defold_dotnet_runtime_identifier(_rid "${platform}")

  execute_process(
    COMMAND "${_dotnet}" --info
    OUTPUT_VARIABLE _dotnet_info
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "Host:[^\n]*\n[ \t]*Version:[ \t]*([0-9][^\n \t]*)" _match "${_dotnet_info}")
  set(_dotnet_version "${CMAKE_MATCH_1}")

  execute_process(
    COMMAND "${_dotnet}" nuget locals global-packages -l
    OUTPUT_VARIABLE _nuget_info
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "global-packages:[ \t]*([^\n]+)" _nuget_match "${_nuget_info}")
  set(_nuget_root "${CMAKE_MATCH_1}")
  string(STRIP "${_nuget_root}" _nuget_root)

  if(NOT _dotnet_version OR NOT _nuget_root)
    return()
  endif()

  set(_aot_base "${_nuget_root}/microsoft.netcore.app.runtime.nativeaot.${_rid}/${_dotnet_version}/runtimes/${_rid}/native")
  if(EXISTS "${_aot_base}/libbootstrapperdll.o")
    set(${out_var} TRUE PARENT_SCOPE)
  endif()
endfunction()

function(defold_target_link_csharp_runtime target platform)
  if(NOT TARGET ${target})
    return()
  endif()

  if(NOT platform MATCHES "-macos$")
    return()
  endif()

  defold_find_dotnet(_dotnet)
  if(NOT _dotnet)
    return()
  endif()

  defold_dotnet_runtime_identifier(_rid "${platform}")

  execute_process(
    COMMAND "${_dotnet}" --info
    OUTPUT_VARIABLE _dotnet_info
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "Host:[^\n]*\n[ \t]*Version:[ \t]*([0-9][^\n \t]*)" _match "${_dotnet_info}")
  set(_dotnet_version "${CMAKE_MATCH_1}")

  execute_process(
    COMMAND "${_dotnet}" nuget locals global-packages -l
    OUTPUT_VARIABLE _nuget_info
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "global-packages:[ \t]*([^\n]+)" _nuget_match "${_nuget_info}")
  set(_nuget_root "${CMAKE_MATCH_1}")
  string(STRIP "${_nuget_root}" _nuget_root)

  if(NOT _dotnet_version OR NOT _nuget_root)
    return()
  endif()

  set(_aot_base "${_nuget_root}/microsoft.netcore.app.runtime.nativeaot.${_rid}/${_dotnet_version}/runtimes/${_rid}/native")
  set(_aot_runtime_files
    "${_aot_base}/libbootstrapperdll.o"
    "${_aot_base}/libRuntime.WorkstationGC.a"
    "${_aot_base}/libeventpipe-enabled.a"
    "${_aot_base}/libstandalonegc-enabled.a"
    "${_aot_base}/libstdc++compat.a"
    "${_aot_base}/libSystem.Native.a"
    "${_aot_base}/libSystem.IO.Compression.Native.a"
    "${_aot_base}/libSystem.Globalization.Native.a")

  if(platform STREQUAL "x86_64-macos")
    list(APPEND _aot_runtime_files "${_aot_base}/libRuntime.VxsortDisabled.a")
  endif()

  foreach(_aot_file IN LISTS _aot_runtime_files)
    if(NOT EXISTS "${_aot_file}")
      message(STATUS "Skipping C# NativeAOT runtime links for ${target}; missing ${_aot_file}")
      return()
    endif()
  endforeach()

  target_link_libraries(${target} PRIVATE ${_aot_runtime_files})

  target_link_libraries(${target} PRIVATE
    "-framework OpenAL"
    "-framework OpenGL"
    "-framework QuartzCore")
endfunction()
