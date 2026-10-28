# MSan needs an instrumented C++ runtime and instrumented external libraries.
# JNI targets opt out through DEFOLD_SKIP_SANITIZER and retain the normal SDK.
set(_msan_platform "${TARGET_PLATFORM}")
if(NOT _msan_platform)
  set(_msan_platform "${HOST_PLATFORM}")
endif()
set(DEFOLD_MSAN_PREFIX "${DEFOLD_SDK_ROOT}/ext/msan/${_msan_platform}" CACHE PATH
  "Install prefix for MSan dependencies")
set(DEFOLD_MSAN_RUNTIME "${DEFOLD_MSAN_PREFIX}/libcxx" CACHE PATH
  "Install prefix for MSan libc++ and libc++abi")
if(NOT EXISTS "${DEFOLD_MSAN_RUNTIME}/lib/libc++.a" OR
   NOT EXISTS "${DEFOLD_MSAN_RUNTIME}/lib/libc++abi.a")
  message(FATAL_ERROR "Missing MSan libc++ in ${DEFOLD_MSAN_RUNTIME}. Run scripts/build.py build_ext -- --with-msan first.")
endif()

set(_msan_enabled "$<NOT:$<BOOL:$<TARGET_PROPERTY:DEFOLD_SKIP_SANITIZER>>>")
target_compile_options(defold_sdk INTERFACE
  "$<$<AND:${_msan_enabled},$<COMPILE_LANGUAGE:CXX>>:-nostdinc++>"
  "$<$<AND:${_msan_enabled},$<COMPILE_LANGUAGE:CXX>>:SHELL:-isystem \"${DEFOLD_MSAN_RUNTIME}/include/c++/v1\">")
target_link_options(defold_sdk INTERFACE
  "$<${_msan_enabled}:-nostdlib++>"
  "$<${_msan_enabled}:LINKER:--compress-debug-sections=zlib>")
target_link_libraries(defold_sdk INTERFACE
  "$<${_msan_enabled}:${DEFOLD_MSAN_RUNTIME}/lib/libc++.a>"
  "$<${_msan_enabled}:${DEFOLD_MSAN_RUNTIME}/lib/libc++abi.a>")
target_link_directories(defold_sdk BEFORE INTERFACE
  "$<${_msan_enabled}:${DEFOLD_MSAN_PREFIX}/lib/${_msan_platform}>")
unset(_msan_enabled)
unset(_msan_platform)
