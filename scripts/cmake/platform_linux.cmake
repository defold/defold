message("platform_linux.cmake:")

# Derive target triple from TARGET_PLATFORM (matches waf_dynamo.py)
if(TARGET_PLATFORM MATCHES "^arm64-")
  set(_DEFOLD_CLANG_TRIPLE "aarch64-unknown-linux-gnu")
else()
  set(_DEFOLD_CLANG_TRIPLE "x86_64-unknown-linux-gnu")
endif()

# Defines
add_compile_definitions(
  DM_PLATFORM_LINUX
)

# Compile options
add_compile_options(
  --target=${_DEFOLD_CLANG_TRIPLE}
)

# C++ specific flags are set globally in platform.cmake (-fno-rtti, etc.)

# Link options
add_link_options(
  --target=${_DEFOLD_CLANG_TRIPLE}
)

