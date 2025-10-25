defold_log("platform_html5.cmake:")

if(NOT TARGET_PLATFORM MATCHES "^(js-web|wasm-web|wasm_pthread-web)$")
  message(FATAL_ERROR "platform_html5.cmake included for non-web TARGET_PLATFORM: ${TARGET_PLATFORM}")
endif()

# Common compile-time definitions (mirrors waf_dynamo for web)
target_compile_definitions(defold_sdk INTERFACE
  GL_ES_VERSION_2_0
  GOOGLE_PROTOBUF_NO_RTTI
  __STDC_LIMIT_MACROS
  DDF_EXPOSE_DESCRIPTORS
  DM_NO_SYSTEM_FUNCTION
  JC_TEST_NO_DEATH_TEST
  PTHREADS_DEBUG
  DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER)

# Common compile options
target_compile_options(defold_sdk INTERFACE
  -Wall
  -fPIC
  -fno-exceptions
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  -Wno-nontrivial-memcall
  -sDISABLE_EXCEPTION_CATCHING=1)

# Threading: wasm_pthread-web uses pthreads
set(_DEFOLD_WITH_PTHREAD OFF)
if(TARGET_PLATFORM STREQUAL "wasm_pthread-web")
  set(_DEFOLD_WITH_PTHREAD ON)
endif()

if(_DEFOLD_WITH_PTHREAD)
  target_compile_options(defold_sdk INTERFACE -pthread)
endif()

# Link options base
set(_DEFOLD_EM_LINK_OPTS
  -Wno-warn-absolute-paths
  --emit-symbol-map
  -lidbfs.js
  -sDISABLE_EXCEPTION_CATCHING=1
  -sALLOW_UNIMPLEMENTED_SYSCALLS=0
  -sEXPORTED_RUNTIME_METHODS=["ccall","UTF8ToString","callMain","HEAPU8","stringToNewUTF8"]
  -sEXPORTED_FUNCTIONS=_main,_malloc,_free
  -sERROR_ON_UNDEFINED_SYMBOLS=1
  -sINITIAL_MEMORY=33554432
  -sMAX_WEBGL_VERSION=2
  -sGL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0
  -sIMPORTED_MEMORY=1
  -sSTACK_SIZE=5MB
)

# Browser minimum versions
if(_DEFOLD_WITH_PTHREAD)
  list(APPEND _DEFOLD_EM_LINK_OPTS
    -sMIN_FIREFOX_VERSION=79
    -sMIN_SAFARI_VERSION=150000
    -sMIN_CHROME_VERSION=75)
else()
  list(APPEND _DEFOLD_EM_LINK_OPTS
    -sMIN_FIREFOX_VERSION=40
    -sMIN_SAFARI_VERSION=101000
    -sMIN_CHROME_VERSION=45)
endif()

# WebGPU support
if(WITH_WEBGPU)
  list(APPEND _DEFOLD_EM_LINK_OPTS
    -sUSE_WEBGPU
    -sGL_WORKAROUND_SAFARI_GETCONTEXT_BUG=0
    -sASYNCIFY
    -sWASM_BIGINT=1)
  # Note: ASYNCIFY_ADVISE etc. are opt-level dependent in waf; omitted here
endif()

# WASM vs asm.js
if(TARGET_PLATFORM MATCHES "^wasm")
  list(APPEND _DEFOLD_EM_LINK_OPTS -sWASM=1 -sALLOW_MEMORY_GROWTH=1)
else()
  list(APPEND _DEFOLD_EM_LINK_OPTS -sWASM=0 -sLEGACY_VM_SUPPORT=1)
endif()

if(_DEFOLD_WITH_PTHREAD)
  list(APPEND _DEFOLD_EM_LINK_OPTS -pthread)
endif()

target_link_options(defold_sdk INTERFACE ${_DEFOLD_EM_LINK_OPTS})
