message("platform_macos.cmake")

# Derive arch from TARGET_PLATFORM
if(TARGET_PLATFORM MATCHES "^arm64-")
  set(_DEFOLD_TARGET_ARCH "arm64")
else()
  set(_DEFOLD_TARGET_ARCH "x86_64")
endif()

# Resolve SDK sysroot and min version if available
set(_DEFOLD_SYSROOT "${CMAKE_OSX_SYSROOT}")
if(NOT _DEFOLD_SYSROOT AND DEFINED ENV{SDKROOT})
  set(_DEFOLD_SYSROOT "$ENV{SDKROOT}")
endif()

# Since it's a cmake feature, let's check it first if it's overridden
set(_DEFOLD_MACOS_MIN "${CMAKE_OSX_DEPLOYMENT_TARGET}")
if(NOT _DEFOLD_MACOS_MIN)
  # Fallback if not provided by the toolchain
  set(_DEFOLD_MACOS_MIN ${SDK_VERSION_MACOSX_MIN})
endif()

# Common compile options for macOS (mirrors waf_dynamo defaults)
add_compile_definitions(
  DM_PLATFORM_MACOS
  GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
  GL_SILENCE_DEPRECATION
)

add_compile_options(
  -mmacosx-version-min=${SDK_VERSION_MACOSX_MIN}
  -target ${_DEFOLD_TARGET_ARCH}-apple-darwin19
)

if(_DEFOLD_SYSROOT)
  add_compile_options(-isysroot ${_DEFOLD_SYSROOT})
  # Add libc++ include dir and disable default stdlib includes for C++
  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-isystem>
    $<$<COMPILE_LANGUAGE:CXX>:${_DEFOLD_SYSROOT}/usr/include/c++/v1>
  )
endif()

# C++ specific
add_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
)

# Link options
set(_DEFOLD_LINK_OPTS
  -stdlib=libc++
  -mmacosx-version-min=${_DEFOLD_MACOS_MIN}

  -target ${_DEFOLD_TARGET_ARCH}-apple-darwin19
)
if(_DEFOLD_SYSROOT)
  list(APPEND _DEFOLD_LINK_OPTS -isysroot ${_DEFOLD_SYSROOT})
endif()

add_link_options(${_DEFOLD_LINK_OPTS})

# Add common macOS frameworks
add_link_options(-Wl,-weak_framework,Foundation)
add_link_options(-framework AppKit)
add_link_options(-framework Carbon)
