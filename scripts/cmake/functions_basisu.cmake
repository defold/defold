# Import the build_ext archives by path so an old dlib-built archive in
# SDK/lib cannot shadow the updated dependency in SDK/ext/lib.
function(defold_import_basisu library platform)
  if(TARGET ${library})
    return()
  endif()

  set(_path "${DEFOLD_SDK_ROOT}/ext/lib/${platform}/${CMAKE_STATIC_LIBRARY_PREFIX}${library}${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "Missing ${_path}. Run ./scripts/build.py --platform=${platform} build_ext first.")
  endif()

  add_library(${library} STATIC IMPORTED GLOBAL)
  set_target_properties(${library} PROPERTIES
    IMPORTED_LOCATION "${_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${DEFOLD_SDK_ROOT}/ext/include"
    INTERFACE_COMPILE_FEATURES cxx_std_17)
  if(library STREQUAL "basis_full")
    if(platform MATCHES "^(x86_64-win32|x86_64-macos)$")
      target_compile_definitions(${library} INTERFACE BASISU_SUPPORT_SSE=1)
    else()
      target_compile_definitions(${library} INTERFACE BASISU_SUPPORT_SSE=0)
    endif()
  else()
    target_compile_definitions(${library} INTERFACE
      BASISD_SUPPORT_KTX2=0 BASISD_SUPPORT_KTX2_ZSTD=0
      BASISD_SUPPORT_XUASTC=0 BASISD_SUPPORT_UASTC_HDR=0)
  endif()
endfunction()
