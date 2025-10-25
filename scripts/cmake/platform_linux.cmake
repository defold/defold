defold_log("platform_linux.cmake:")

# Derive target triple from TARGET_PLATFORM (matches waf_dynamo.py)
if(TARGET_PLATFORM MATCHES "^arm64-")
  set(_DEFOLD_CLANG_TRIPLE "aarch64-unknown-linux-gnu")
else()
  set(_DEFOLD_CLANG_TRIPLE "x86_64-unknown-linux-gnu")
endif()

# Defines
target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_LINUX)

# Compile options
target_compile_options(defold_sdk INTERFACE --target=${_DEFOLD_CLANG_TRIPLE})

# C++ specific flags are set globally in platform.cmake (-fno-rtti, etc.)

# Link options
target_link_options(defold_sdk INTERFACE
  --target=${_DEFOLD_CLANG_TRIPLE}
  -fuse-ld=lld)
