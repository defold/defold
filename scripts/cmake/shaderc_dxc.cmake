include_guard(GLOBAL)

# Shared by shaderc and the graphics tests, including focused graphics builds.
# Engine runtime targets do not link the compiler.
function(shaderc_import_dxc)
  if(TARGET shaderc_dxcompiler)
    return()
  endif()
  add_library(shaderc_dxcompiler SHARED IMPORTED GLOBAL)
  set(_dxc_includes "${DEFOLD_EXT_INCLUDE_DIR}")
  if(TARGET_PLATFORM MATCHES "win32$")
    set(_dxc_runtime "${DEFOLD_SDK_ROOT}/ext/bin/${TARGET_PLATFORM}/dxcompiler.dll")
    set_target_properties(shaderc_dxcompiler PROPERTIES
      IMPORTED_IMPLIB "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}/dxcompiler.lib")
    list(APPEND _dxc_includes "${DEFOLD_EXT_INCLUDE_DIR}/directx")
  else()
    set(_dxc_runtime "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}/${CMAKE_SHARED_LIBRARY_PREFIX}dxcompiler${CMAKE_SHARED_LIBRARY_SUFFIX}")
    list(APPEND _dxc_includes
      "${DEFOLD_EXT_INCLUDE_DIR}/dxc/DirectX-Headers"
      "${DEFOLD_EXT_INCLUDE_DIR}/dxc/DirectX-Headers/wsl/stubs"
      "${DEFOLD_EXT_INCLUDE_DIR}/dxc/DirectX-Headers/directx")
  endif()
  set_target_properties(shaderc_dxcompiler PROPERTIES
    IMPORTED_LOCATION "${_dxc_runtime}"
    INTERFACE_INCLUDE_DIRECTORIES "${_dxc_includes}")
endfunction()

function(shaderc_stage_dxc target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:shaderc_dxcompiler>" "$<TARGET_FILE_DIR:${target}>"
    VERBATIM)
  if(APPLE)
    set_target_properties(${target} PROPERTIES SKIP_BUILD_RPATH FALSE BUILD_RPATH "@loader_path" INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_target_properties(${target} PROPERTIES SKIP_BUILD_RPATH FALSE BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
  endif()
endfunction()
