cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_engine LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="ENGINE")

set(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(ENGINE_SRC_DIR "${ENGINE_ROOT}/src")
set(ENGINE_PROTO_DIR "${ENGINE_ROOT}/proto")
set(ENGINE_CONTENT_DIR "${ENGINE_ROOT}/content")
set(ENGINE_DDF_GENERATED_DEPENDENCIES gameobject)
set(ENGINE_SERVICE_DEPENDENCIES gameobject gamesys gui)
set(ENGINE_DEPENDENCIES
  gameobject
  gamesys
  gui
  input
  lua
  particle
  render
  script)
set(ENGINE_EXECUTABLE_PREFIX_DEPENDENCIES
  gameobject
  ddf
  liveupdate
  resource)
set(ENGINE_EXECUTABLE_SUFFIX_DEPENDENCIES
  render
  render_font_default
  script
  extension
  input
  particle
  rig
  dlib
  zip
  mbedtls
  gui
  crashext)
set(ENGINE_NULL_GRAPHICS_EXECUTABLE_DEPENDENCIES
  record_null
  graphics_null
  image
  graphics_transcoder_null
  hid_null
  platform_null
  sound_null)
set(ENGINE_RECORD_EXECUTABLE_DEPENDENCIES record vpx hid)
set(ENGINE_RECORD_NULL_EXECUTABLE_DEPENDENCIES record_null hid)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Java REQUIRED COMPONENTS Runtime)

execute_process(
  COMMAND git log --pretty=%H -n1
  WORKING_DIRECTORY "${DEFOLD_HOME}"
  OUTPUT_VARIABLE ENGINE_SHA1
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT ENGINE_SHA1)
  set(ENGINE_SHA1 "unknown")
endif()

set(ENGINE_PRIVATE_SHA1 "")
if(DEFOLD_PRIVATE_REPO_ROOT)
  execute_process(
    COMMAND git -c "safe.directory=${DEFOLD_PRIVATE_REPO_ROOT}" -C "${DEFOLD_PRIVATE_REPO_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE ENGINE_PRIVATE_SHA1
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE ENGINE_PRIVATE_SHA1_ERROR
    RESULT_VARIABLE ENGINE_PRIVATE_SHA1_RESULT)
  if(NOT ENGINE_PRIVATE_SHA1_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not resolve private engine sha1 for ${TARGET_PLATFORM}: ${ENGINE_PRIVATE_SHA1_ERROR}")
  endif()
endif()

file(READ "${DEFOLD_HOME}/VERSION" ENGINE_VERSION)
string(STRIP "${ENGINE_VERSION}" ENGINE_VERSION)

set(ENGINE_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/engine_version.h")
file(GENERATE
  OUTPUT "${ENGINE_VERSION_HEADER}"
  CONTENT "namespace dmEngineVersion\n{\n    static const char* PLATFORM = \"${TARGET_PLATFORM}\";\n    static const char* VERSION = \"${ENGINE_VERSION}\";\n    static const char* VERSION_SHA1 = \"${ENGINE_SHA1}\";\n    static const char* PRIVATE_VERSION_SHA1 = \"${ENGINE_PRIVATE_SHA1}\";\n}\n")

set(ENGINE_DDF_PROTO "${ENGINE_PROTO_DIR}/engine/engine_ddf.proto")
set(ENGINE_DDF_CPP "${CMAKE_CURRENT_BINARY_DIR}/engine/engine_ddf.cpp")
set(ENGINE_DDF_H "${CMAKE_CURRENT_BINARY_DIR}/engine/engine_ddf.h")
defold_protoc_gen_cpp("${ENGINE_DDF_CPP}" "${ENGINE_DDF_PROTO}"
  INCLUDES "${ENGINE_PROTO_DIR}" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${CMAKE_CURRENT_BINARY_DIR}/proto/engine/engine_ddf_pb2.py" "${ENGINE_DDF_PROTO}"
  INCLUDES "${ENGINE_PROTO_DIR}" "${DEFOLD_SDK_ROOT}/share/proto")
add_custom_target(engine_proto_py ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/proto/engine/engine_ddf_pb2.py")

defold_add_library(engine_ddf_generated OBJECT "${ENGINE_DDF_CPP}")
target_include_directories(engine_ddf_generated PUBLIC
  "${ENGINE_SRC_DIR}"
  "${ENGINE_ROOT}"
  "${ENGINE_PROTO_DIR}"
  "${DEFOLD_HOME}/engine/record/src"
  "${DEFOLD_GAMEOBJECT_BINARY_DIR}/src/proto"
  "${CMAKE_CURRENT_BINARY_DIR}")
defold_target_link_libraries(engine_ddf_generated "${TARGET_PLATFORM}" SCOPE PUBLIC ${ENGINE_DDF_GENERATED_DEPENDENCIES})
if(TARGET gameobject_proto_headers)
  add_dependencies(engine_ddf_generated gameobject_proto_headers)
endif()

set(ENGINE_CMAKE_CONTENT_SCRIPT "${ENGINE_ROOT}/scripts/build_cmake_content.py")
set(ENGINE_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
file(GLOB ENGINE_BOB_PLUGIN_JARS CONFIGURE_DEPENDS "${DEFOLD_SDK_ROOT}/share/java/plugins/*.jar")
set(ENGINE_BOB_CLASSPATH_LIST "${ENGINE_BOB_LIGHT}" ${ENGINE_BOB_PLUGIN_JARS})
cmake_path(CONVERT "${ENGINE_BOB_CLASSPATH_LIST}" TO_NATIVE_PATH_LIST ENGINE_BOB_CLASSPATH)

file(GLOB_RECURSE ENGINE_CONTENT_SOURCES CONFIGURE_DEPENDS
  "${ENGINE_CONTENT_DIR}/builtins/*"
  "${ENGINE_CONTENT_DIR}/materials/*"
  "${ENGINE_CONTENT_DIR}/profiler.html")

set(ENGINE_BUILTINS_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/content-work")
set(ENGINE_BUILTINS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/content")
set(ENGINE_BUILTINS_COMPILED_DIR "${ENGINE_BUILTINS_WORK_ROOT}/build/default/builtins")
set(ENGINE_BUILTINS_STAMP "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.stamp")
defold_get_graphics_symbols(ENGINE_GRAPHICS_SYMBOLS "${TARGET_PLATFORM}")
set(ENGINE_SHADER_OUTPUT_ARGS)
# App manifests can relink Apple engines to GL/GLES even when the default
# CMake graphics symbols only include Vulkan/Metal-capable adapters.
if("GraphicsAdapterOpenGL" IN_LIST ENGINE_GRAPHICS_SYMBOLS
    OR "GraphicsAdapterOpenGLES" IN_LIST ENGINE_GRAPHICS_SYMBOLS
    OR TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
  list(APPEND ENGINE_SHADER_OUTPUT_ARGS --shader-output glsl)
endif()
# App manifests can also relink Apple engines to Vulkan/MoltenVK.
if("GraphicsAdapterVulkan" IN_LIST ENGINE_GRAPHICS_SYMBOLS
    OR TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
  list(APPEND ENGINE_SHADER_OUTPUT_ARGS --shader-output spirv)
endif()
if("GraphicsAdapterWebGPU" IN_LIST ENGINE_GRAPHICS_SYMBOLS)
  list(APPEND ENGINE_SHADER_OUTPUT_ARGS --shader-output wgsl)
endif()
if("GraphicsAdapterDX12" IN_LIST ENGINE_GRAPHICS_SYMBOLS)
  list(APPEND ENGINE_SHADER_OUTPUT_ARGS --shader-output hlsl)
endif()
if("GraphicsAdapterMetal" IN_LIST ENGINE_GRAPHICS_SYMBOLS
    OR TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
  list(APPEND ENGINE_SHADER_OUTPUT_ARGS --shader-output msl)
endif()
add_custom_command(
  OUTPUT
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arci"
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arcd"
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.dmanifest"
    "${ENGINE_BUILTINS_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E env "DM_JAVA_RUNTIME_FLAGS=${DEFOLD_JAVA_RUNTIME_FLAGS}"
    "${Python3_EXECUTABLE}" "${ENGINE_CMAKE_CONTENT_SCRIPT}" builtins
    --source-root "${ENGINE_CONTENT_DIR}"
    --work-root "${ENGINE_BUILTINS_WORK_ROOT}"
    --output-root "${ENGINE_BUILTINS_OUTPUT_DIR}"
    --stamp "${ENGINE_BUILTINS_STAMP}"
    --bob-light "${ENGINE_BOB_LIGHT}"
    --bob-classpath "${ENGINE_BOB_CLASSPATH}"
    --platform "${TARGET_PLATFORM}"
    ${ENGINE_SHADER_OUTPUT_ARGS}
    --java "${Java_JAVA_EXECUTABLE}"
  DEPENDS "${ENGINE_CMAKE_CONTENT_SCRIPT}" "${ENGINE_BOB_LIGHT}" ${ENGINE_BOB_PLUGIN_JARS} ${ENGINE_CONTENT_SOURCES}
  COMMENT "Building engine builtin archive"
  VERBATIM)
add_custom_target(engine_builtins
  DEPENDS
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arci"
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arcd"
    "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.dmanifest")
if(TARGET bob_light)
  add_dependencies(engine_builtins bob_light)
endif()

set(ENGINE_DEBUG_SPC "${CMAKE_CURRENT_BINARY_DIR}/content/materials/debug.spc")
add_custom_command(
  OUTPUT "${ENGINE_DEBUG_SPC}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/content/materials"
  COMMAND "${Java_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST} -cp "${ENGINE_BOB_CLASSPATH}"
    com.dynamo.bob.pipeline.ShaderProgramBuilder
    "${ENGINE_CONTENT_DIR}/materials/debug.fp"
    "${ENGINE_CONTENT_DIR}/materials/debug.vp"
    "${ENGINE_DEBUG_SPC}"
    "${TARGET_PLATFORM}"
    "${ENGINE_CONTENT_DIR}"
  DEPENDS
    "${ENGINE_CONTENT_DIR}/materials/debug.fp"
    "${ENGINE_CONTENT_DIR}/materials/debug.vp"
    "${ENGINE_CONTENT_DIR}/materials/debug.material"
    "${ENGINE_BOB_LIGHT}"
    ${ENGINE_BOB_PLUGIN_JARS}
  COMMENT "Building engine debug shader"
  VERBATIM)

defold_embed_to_header(
  INPUT "${ENGINE_CONTENT_DIR}/profiler.html"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/profiler.html.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/profiler.html.embed.cpp")
defold_embed_to_header(
  INPUT "${ENGINE_DEBUG_SPC}"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/materials/debug.spc.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/materials/debug.spc.embed.cpp")
defold_embed_to_header(
  INPUT "${ENGINE_CONTENT_DIR}/builtins/connect/game.project"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/builtins/connect/game.project.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/builtins/connect/game.project.embed.cpp")
defold_embed_to_header(
  INPUT "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arci"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arci.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arci.embed.cpp")
defold_embed_to_header(
  INPUT "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arcd"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arcd.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arcd.embed.cpp")
defold_embed_to_header(
  INPUT "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.dmanifest"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.dmanifest.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.dmanifest.embed.cpp")

set(ENGINE_PROFILER_EMBED_CPP "${CMAKE_CURRENT_BINARY_DIR}/content/profiler.html.embed.cpp")
set(ENGINE_EMBED_CPP
  "${CMAKE_CURRENT_BINARY_DIR}/content/materials/debug.spc.embed.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/content/builtins/connect/game.project.embed.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arci.embed.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.arcd.embed.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/content/builtins.dmanifest.embed.cpp")

set(ENGINE_SERVICE_SOURCES
  "${ENGINE_SRC_DIR}/engine_service.cpp"
  "${ENGINE_SRC_DIR}/engine_service_discovery.cpp")
if(TARGET_PLATFORM MATCHES "ios")
  list(APPEND ENGINE_SERVICE_SOURCES "${ENGINE_SRC_DIR}/engine_service_discovery_ios.mm")
endif()

defold_add_library(engine_service STATIC ${ENGINE_SERVICE_SOURCES} "${ENGINE_PROFILER_EMBED_CPP}")
target_include_directories(engine_service PUBLIC "${ENGINE_SRC_DIR}" "${ENGINE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")
foreach(_engine_service_dependency IN ITEMS
    gamesys_proto_headers
    graphics_proto_header
    input_proto_headers
    particle_proto_headers
    render_proto_headers)
  if(TARGET ${_engine_service_dependency})
    add_dependencies(engine_service ${_engine_service_dependency})
  endif()
endforeach()
defold_target_link_libraries(engine_service "${TARGET_PLATFORM}" SCOPE PUBLIC ${ENGINE_SERVICE_DEPENDENCIES})

defold_add_library(engine_service_null STATIC "${ENGINE_SRC_DIR}/engine_service_null.cpp")
target_include_directories(engine_service_null PUBLIC "${ENGINE_SRC_DIR}" "${ENGINE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")

set(ENGINE_COMMON_SOURCES
  "${ENGINE_SRC_DIR}/engine.cpp"
  "${ENGINE_SRC_DIR}/engine_main.cpp"
  "${ENGINE_SRC_DIR}/engine_loop.cpp"
  "${ENGINE_SRC_DIR}/extension.cpp"
  "${ENGINE_SRC_DIR}/script/script_engine.cpp")
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  list(APPEND ENGINE_COMMON_SOURCES "${ENGINE_SRC_DIR}/engine_web.cpp")
elseif(TARGET_PLATFORM MATCHES "arm64-nx64")
  list(APPEND ENGINE_COMMON_SOURCES
    "${ENGINE_SRC_DIR}/nx64/main_nx64.cpp"
    "${ENGINE_SRC_DIR}/nx64/engine_nx64.cpp")
elseif(TARGET_PLATFORM MATCHES "x86_64-ps4|x86_64-ps5")
  list(APPEND ENGINE_COMMON_SOURCES "${ENGINE_SRC_DIR}/engine_ps4.cpp")
elseif(TARGET_PLATFORM MATCHES "x86_64-xbone")
  list(APPEND ENGINE_COMMON_SOURCES "${ENGINE_SRC_DIR}/engine_xbox.cpp")
endif()

set(ENGINE_DEFINES)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  list(APPEND ENGINE_DEFINES DM_NO_HTTP_CACHE)
endif()

defold_add_library(engine STATIC
  ${ENGINE_COMMON_SOURCES}
  "${ENGINE_SRC_DIR}/physics_debug_render.cpp"
  $<TARGET_OBJECTS:engine_ddf_generated>
  ${ENGINE_EMBED_CPP})
target_include_directories(engine PUBLIC
  "${ENGINE_SRC_DIR}"
  "${ENGINE_ROOT}"
  "${ENGINE_PROTO_DIR}"
  "${DEFOLD_HOME}/engine/record/src"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(engine PRIVATE ${ENGINE_DEFINES})
add_dependencies(engine engine_builtins)
add_dependencies(engine engine_ddf_generated)
foreach(_engine_dependency IN ITEMS
    gamesys_proto_headers
    gameobject_proto_headers
    graphics_proto_header
    input_proto_headers
    particle_proto_headers
    render_proto_headers)
  if(TARGET ${_engine_dependency})
    add_dependencies(engine ${_engine_dependency})
  endif()
endforeach()
add_dependencies(engine
  profilerext_sdk_headers
  crashext_sdk_headers
  liveupdate_sdk_headers)
defold_target_link_libraries(engine "${TARGET_PLATFORM}" SCOPE PUBLIC ${ENGINE_DEPENDENCIES})
target_link_libraries(engine PUBLIC engine_service)

defold_add_library(engine_release STATIC ${ENGINE_COMMON_SOURCES} $<TARGET_OBJECTS:engine_ddf_generated>)
target_include_directories(engine_release PUBLIC
  "${ENGINE_SRC_DIR}"
  "${ENGINE_ROOT}"
  "${ENGINE_PROTO_DIR}"
  "${DEFOLD_HOME}/engine/record/src"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(engine_release PRIVATE DM_RELEASE=1 ${ENGINE_DEFINES})
add_dependencies(engine_release engine_ddf_generated)
foreach(_engine_dependency IN ITEMS
    gamesys_proto_headers
    gameobject_proto_headers
    graphics_proto_header
    input_proto_headers
    particle_proto_headers
    render_proto_headers)
  if(TARGET ${_engine_dependency})
    add_dependencies(engine_release ${_engine_dependency})
  endif()
endforeach()
add_dependencies(engine_release
  profilerext_sdk_headers
  crashext_sdk_headers
  liveupdate_sdk_headers)
defold_target_link_libraries(engine_release "${TARGET_PLATFORM}" SCOPE PUBLIC ${ENGINE_DEPENDENCIES})
set_target_properties(engine_release PROPERTIES DEFOLD_SDK_HEADERS_TARGET engine_sdk_headers)
target_link_libraries(engine_release PUBLIC engine_service_null)

defold_feature_enabled(box2dv3 ENGINE_WITH_BOX2DV3)
defold_feature_enabled(font_layout ENGINE_WITH_FONT_LAYOUT)
defold_feature_enabled(simd ENGINE_WITH_SIMD)

defold_get_gamesys_libraries(ENGINE_GAMESYS_LIBS)
defold_get_font_libraries(ENGINE_FONT_LIBS)

set(ENGINE_SOUND_LIBS sound decoder_wav decoder_ogg)
if(WITH_OPENAL)
  set(ENGINE_SOUND_LIBS sound_openal decoder_wav decoder_ogg)
elseif(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$" AND NOT ENGINE_WITH_SIMD)
  set(ENGINE_SOUND_LIBS sound_nosimd decoder_wav decoder_ogg)
endif()
if(WITH_OPUS)
  list(APPEND ENGINE_SOUND_LIBS opus decoder_opus)
endif()

set(ENGINE_EXTRA_LIBS crashext liveupdate ${ENGINE_FONT_LIBS})
if(TARGET_PLATFORM MATCHES "android")
  list(APPEND ENGINE_EXTRA_LIBS unwind)
endif()
if(TARGET_PLATFORM MATCHES "android")
  list(APPEND ENGINE_EXTRA_LIBS c++_static)
endif()
if(NOT TARGET_PLATFORM_OS STREQUAL "win32" AND
   NOT TARGET_PLATFORM MATCHES "^(arm64-nx64|wasm-web|wasm_pthread-web|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  list(APPEND ENGINE_EXTRA_LIBS tremolo)
endif()

set(ENGINE_RESOURCE_SYMBOLS
  ResourceTypeGameObject ResourceTypeCollection ResourceTypeScript ResourceTypeLua
  ResourceTypeAnim ResourceTypeAnimationSet ResourceTypeGui ResourceTypeGuiScript
  ResourceTypeOgg ResourceTypeOpus ResourceTypeWav ResourceTypeFont ResourceTypeTTF
  ResourceProviderFile ResourceProviderArchive ResourceProviderZip ResourceTypeLight)
set(ENGINE_COMPONENT_SYMBOLS
  ComponentTypeAnim ComponentTypeScript ComponentTypeGui ComponentTypeMesh ComponentTypeSound ComponentTypeLight)
set(ENGINE_EXPORTED_SYMBOLS
  DefaultSoundDevice AudioDecoderWav CrashExt LiveUpdateExt
  ScriptBox2DExt ScriptImageExt ScriptModelExt ScriptTypesExt ScriptHttp ScriptFont)
if(TARGET_PLATFORM_OS STREQUAL "win32" OR TARGET_PLATFORM MATCHES "^(arm64-nx64|wasm-web|wasm_pthread-web)$")
  list(APPEND ENGINE_EXPORTED_SYMBOLS AudioDecoderStbVorbis)
elseif(NOT TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  list(APPEND ENGINE_EXPORTED_SYMBOLS AudioDecoderTremolo)
endif()
if(WITH_OPUS)
  list(APPEND ENGINE_EXPORTED_SYMBOLS AudioDecoderOpus)
endif()
list(APPEND ENGINE_EXPORTED_SYMBOLS ${ENGINE_GRAPHICS_SYMBOLS})

set(ENGINE_PROFILE_LIBS profile profilerext)
set(ENGINE_PROFILE_SYMBOLS ProfilerExt ProfilerBasic)
if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  list(APPEND ENGINE_PROFILE_LIBS profiler_remotery)
  list(APPEND ENGINE_PROFILE_SYMBOLS ProfilerRemotery)
endif()
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  list(APPEND ENGINE_PROFILE_LIBS profiler_js)
  list(APPEND ENGINE_PROFILE_SYMBOLS ProfilerJS)
endif()

set(ENGINE_WEB_LIBS library_sys.js library_script.js library_render.js)
if(NOT WITH_OPENAL)
  list(APPEND ENGINE_WEB_LIBS library_sound.js)
endif()
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  list(APPEND ENGINE_WEB_LIBS library_profile.js)
endif()

function(engine_configure_executable target variant engine_lib engine_service_lib graphics_mode profile_libs profile_symbols)
  target_include_directories(${target} PRIVATE
    "${ENGINE_SRC_DIR}"
    "${ENGINE_ROOT}"
    "${ENGINE_PROTO_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${target} PRIVATE _XBOX)
  if(variant STREQUAL "release")
    target_compile_definitions(${target} PRIVATE DM_RELEASE=1)
  endif()
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    ${engine_lib}
    ${engine_service_lib}
    ${profile_libs}
    ${ENGINE_EXECUTABLE_PREFIX_DEPENDENCIES}
    ${ENGINE_GAMESYS_LIBS}
    ${ENGINE_EXECUTABLE_SUFFIX_DEPENDENCIES}
    ${ENGINE_EXTRA_LIBS})
  defold_get_physics_libraries(_engine_physics_libs)
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${_engine_physics_libs})
  defold_target_link_bullet3d(${target} "${TARGET_PLATFORM}")
  defold_target_link_lua(${target} "${TARGET_PLATFORM}")
  defold_target_link_socket(${target} "${TARGET_PLATFORM}")
  defold_target_link_app(${target} "${TARGET_PLATFORM}")
  if(graphics_mode STREQUAL "null")
    defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
      ${ENGINE_NULL_GRAPHICS_EXECUTABLE_DEPENDENCIES})
    if(TARGET_PLATFORM MATCHES "^(arm64-android|armv7-android)$")
      defold_target_link_libraries(${target} "${TARGET_PLATFORM}" dmglfw EGL GLESv1_CM GLESv2)
    endif()
  else()
    if(variant STREQUAL "release")
      defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${ENGINE_RECORD_NULL_EXECUTABLE_DEPENDENCIES} ${ENGINE_SOUND_LIBS})
    elseif(TARGET record)
      defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${ENGINE_RECORD_EXECUTABLE_DEPENDENCIES} ${ENGINE_SOUND_LIBS})
    else()
      defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${ENGINE_RECORD_NULL_EXECUTABLE_DEPENDENCIES} ${ENGINE_SOUND_LIBS})
    endif()
    defold_target_link_platform(${target} "${TARGET_PLATFORM}")
    defold_target_link_graphics(${target} "${TARGET_PLATFORM}")
  endif()
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}" SCOPE PRIVATE ${ENGINE_WEB_LIBS})
  if(TARGET_PLATFORM MATCHES "win32")
    target_sources(${target} PRIVATE "${ENGINE_SRC_DIR}/engine.rc")
    target_link_libraries(${target} PRIVATE Psapi.lib)
    target_link_options(${target} PRIVATE /DEBUG)
    if(variant STREQUAL "release")
      target_link_options(${target} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
  elseif(TARGET_PLATFORM MATCHES "macos")
    target_link_libraries(${target} PRIVATE "-framework AVFoundation" "-framework GameController")
  elseif(TARGET_PLATFORM MATCHES "ios")
    target_link_libraries(${target} PRIVATE "-framework AVFoundation" "-framework UIKit" "-framework OpenGLES" "-framework QuartzCore" "-framework CoreGraphics")
  endif()
endfunction()

set(ENGINE_MAIN_SOURCES "${ENGINE_SRC_DIR}/common/main.cpp")
if(TARGET_PLATFORM MATCHES "^(arm64-android|armv7-android)$")
  list(APPEND ENGINE_MAIN_SOURCES "${ENGINE_SRC_DIR}/common/android_main.cpp")
endif()

function(engine_add_binary target)
  if(TARGET_PLATFORM MATCHES "^(arm64-android|armv7-android)$")
    # NativeActivity loads Android engines with dlopen/dlsym, so these must be
    # true shared libraries rather than PIE executables renamed to .so files.
    defold_add_library(${target} SHARED ${ARGN})
  else()
    defold_add_executable(${target} ${ARGN})
  endif()
endfunction()

set(ENGINE_DMENGINE_SYMBOLS
  ${ENGINE_EXPORTED_SYMBOLS}
  ${ENGINE_PROFILE_SYMBOLS}
  ${ENGINE_RESOURCE_SYMBOLS}
  ResourceProviderHttp
  ${ENGINE_COMPONENT_SYMBOLS})
set(ENGINE_DMENGINE_EXPORTS "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_dmengine.cpp")
defold_create_exported_symbols_file("${ENGINE_DMENGINE_EXPORTS}" "${ENGINE_DMENGINE_SYMBOLS}")
engine_add_binary(dmengine ${ENGINE_MAIN_SOURCES} "${ENGINE_DMENGINE_EXPORTS}")
engine_configure_executable(dmengine debug engine engine_service graphics "${ENGINE_PROFILE_LIBS}" "${ENGINE_PROFILE_SYMBOLS}")
defold_xcode_configure_ios_app(dmengine
  BUNDLE_ID "${DEFOLD_IOS_BUNDLE_IDENTIFIER}"
  VERSION "${ENGINE_VERSION}")
defold_codesign_target(dmengine darwin/entitlements-macos-debug.plist)

set(ENGINE_RELEASE_SYMBOLS
  ${ENGINE_EXPORTED_SYMBOLS}
  ProfilerExt
  ${ENGINE_RESOURCE_SYMBOLS}
  ${ENGINE_COMPONENT_SYMBOLS})
set(ENGINE_RELEASE_EXPORTS "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_dmengine_release.cpp")
defold_create_exported_symbols_file("${ENGINE_RELEASE_EXPORTS}" "${ENGINE_RELEASE_SYMBOLS}")
engine_add_binary(dmengine_release ${ENGINE_MAIN_SOURCES} "${ENGINE_RELEASE_EXPORTS}")
engine_configure_executable(dmengine_release release engine_release engine_service_null graphics "profile_null;profilerext_null" "ProfilerExt")
defold_codesign_target(dmengine_release darwin/entitlements-macos-release.plist)

set(ENGINE_HEADLESS_SYMBOLS
  LiveUpdateExt
  ScriptTypesExt
  GraphicsAdapterNull
  ${ENGINE_PROFILE_SYMBOLS}
  ${ENGINE_RESOURCE_SYMBOLS}
  ${ENGINE_COMPONENT_SYMBOLS})
set(ENGINE_HEADLESS_EXPORTS "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_dmengine_headless.cpp")
defold_create_exported_symbols_file("${ENGINE_HEADLESS_EXPORTS}" "${ENGINE_HEADLESS_SYMBOLS}")
engine_add_binary(dmengine_headless ${ENGINE_MAIN_SOURCES} "${ENGINE_HEADLESS_EXPORTS}")
engine_configure_executable(dmengine_headless headless engine engine_service null "${ENGINE_PROFILE_LIBS}" "${ENGINE_PROFILE_SYMBOLS}")
defold_codesign_target(dmengine_headless darwin/entitlements-macos-debug.plist)

if(TARGET_PLATFORM MATCHES "^(arm64-android|armv7-android)$")
  set_target_properties(dmengine dmengine_release dmengine_headless PROPERTIES
    PREFIX "lib"
    SUFFIX ".so")

  set(ENGINE_ANDROID_JARS_INSTALL_CODE "
      \"\${CMAKE_INSTALL_PREFIX}/ext/share/java/glfw_android.jar\"
      \"\${CMAKE_INSTALL_PREFIX}/share/java/gamesys_android.jar\"
      \"\${CMAKE_INSTALL_PREFIX}/share/java/sound_android.jar\"")
  list(LENGTH ENGINE_GRAPHICS_SYMBOLS ENGINE_GRAPHICS_SYMBOLS_COUNT)
  if("GraphicsAdapterVulkan" IN_LIST ENGINE_GRAPHICS_SYMBOLS AND ENGINE_GRAPHICS_SYMBOLS_COUNT GREATER 1)
    string(APPEND ENGINE_ANDROID_JARS_INSTALL_CODE "
      \"\${CMAKE_INSTALL_PREFIX}/ext/share/java/vkquality.jar\"")
  endif()

  install(CODE "
    set(_dex_dir \"\${CMAKE_INSTALL_PREFIX}/share/java/classes-dex\")
    set(_classes_dex \"\${_dex_dir}/classes.dex\")
    set(_android_jars ${ENGINE_ANDROID_JARS_INSTALL_CODE})
    foreach(_android_jar IN LISTS _android_jars)
      if(NOT EXISTS \"\${_android_jar}\")
        message(FATAL_ERROR \"Android dex input is missing: \${_android_jar}\")
      endif()
    endforeach()
    file(REMOVE_RECURSE \"\${_dex_dir}\")
    file(MAKE_DIRECTORY \"\${_dex_dir}\")
    execute_process(
      COMMAND \"${DEFOLD_ANDROID_D8}\" --lib \"${DEFOLD_ANDROID_JAR}\" --output \"\${_dex_dir}\" \${_android_jars}
      RESULT_VARIABLE _d8_result)
    if(NOT _d8_result EQUAL 0 OR NOT EXISTS \"\${_classes_dex}\")
      message(FATAL_ERROR \"Failed to generate Android classes.dex\")
    endif()
    file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/share/java\" TYPE FILE FILES \"\${_classes_dex}\")
    file(REMOVE_RECURSE \"\${_dex_dir}\")")
endif()

defold_install_targets(TARGETS engine engine_release engine_service engine_service_null
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_install_targets(TARGETS dmengine dmengine_release dmengine_headless
  LIBRARY DESTINATION bin/${TARGET_PLATFORM}
  RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if(TARGET_PLATFORM MATCHES "win32|x86_64-xbone")
  foreach(_engine_pdb_target IN ITEMS dmengine dmengine_release dmengine_headless)
    install(FILES "$<TARGET_PDB_FILE:${_engine_pdb_target}>"
      DESTINATION bin/${TARGET_PLATFORM}
      OPTIONAL)
  endforeach()
endif()

if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  set(ENGINE_WEB_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  install(CODE "
    foreach(_name IN ITEMS dmengine dmengine_release dmengine_headless)
      foreach(_suffix IN ITEMS .js.mem .js.symbols .js.map .wasm .wasm.map .wasm.debug.wasm)
        set(_file \"${ENGINE_WEB_OUTPUT_DIR}/\${_name}\${_suffix}\")
        if(EXISTS \"\${_file}\")
          file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin/${TARGET_PLATFORM}\" TYPE FILE FILES \"\${_file}\")
        endif()
      endforeach()
    endforeach()")
endif()

file(GLOB_RECURSE ENGINE_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${ENGINE_ROOT}/src/dmsdk/*.h"
  "${ENGINE_ROOT}/src/dmsdk/*.hpp")
defold_add_sdk_headers_target(engine_public_sdk_headers
  DESTINATION include/engine
  SOURCE_ROOT "${ENGINE_SRC_DIR}"
  FILES "${ENGINE_SRC_DIR}/engine.h"
  DEPENDS engine)
defold_add_sdk_headers_target(engine_ddf_sdk_headers
  DESTINATION include/engine
  FILES "${ENGINE_DDF_H}"
  DEPENDS engine)
defold_add_sdk_headers_target(engine_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${ENGINE_ROOT}/src/dmsdk"
  FILES ${ENGINE_DMSDK_HEADERS}
  DEPENDS engine)
set(ENGINE_SDK_HEADER_TARGETS engine_public_sdk_headers engine_ddf_sdk_headers engine_dmsdk_sdk_headers)
list(APPEND ENGINE_SDK_HEADER_TARGETS
  crashext_sdk_headers
  extension_sdk_headers
  gameobject_sdk_headers
  gamesys_sdk_headers
  graphics_sdk_headers
  gui_sdk_headers
  input_sdk_headers
  liveupdate_sdk_headers
  particle_sdk_headers
  profilerext_sdk_headers
  render_sdk_headers
  resource_sdk_headers
  script_sdk_headers)
add_custom_target(engine_sdk_headers
  DEPENDS ${ENGINE_SDK_HEADER_TARGETS})

install(FILES "${ENGINE_SRC_DIR}/engine.h" "${ENGINE_DDF_H}" DESTINATION include/engine)
install(FILES "${ENGINE_PROTO_DIR}/engine/engine_ddf.proto" DESTINATION share/proto/engine)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/proto/engine/engine_ddf_pb2.py" DESTINATION lib/python/engine)
if(TARGET_PLATFORM MATCHES "win32$")
  install(FILES "${ENGINE_SRC_DIR}/defold.ico" "${ENGINE_SRC_DIR}/engine.rc" DESTINATION lib/${TARGET_PLATFORM})
endif()
install(FILES "${ENGINE_ROOT}/scripts/dmengine_memprofile.sh"
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY "${ENGINE_ROOT}/src/dmsdk/"
  DESTINATION sdk/include/dmsdk
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp")
install(FILES
  "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arci"
  "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.arcd"
  "${ENGINE_BUILTINS_OUTPUT_DIR}/builtins.dmanifest"
  DESTINATION content)
install(DIRECTORY "${ENGINE_CONTENT_DIR}/builtins/"
  DESTINATION content/builtins)

if(BUILD_TESTS)
  set(ENGINE_BOB_ENGINE_JAR "${CMAKE_CURRENT_BINARY_DIR}/content/bob-engine.jar")
  add_custom_command(
    OUTPUT "${ENGINE_BOB_ENGINE_JAR}"
    COMMAND "${Python3_EXECUTABLE}" "${ENGINE_CMAKE_CONTENT_SCRIPT}" package-bob
      --source-root "${ENGINE_CONTENT_DIR}"
      --output "${ENGINE_BOB_ENGINE_JAR}"
      --bob-light "${ENGINE_BOB_LIGHT}"
    DEPENDS "${ENGINE_CMAKE_CONTENT_SCRIPT}" "${ENGINE_BOB_LIGHT}" ${ENGINE_CONTENT_SOURCES}
    COMMENT "Packaging bob-engine.jar"
    VERBATIM)
  add_custom_target(engine_bob_engine_jar ALL DEPENDS "${ENGINE_BOB_ENGINE_JAR}")
  if(TARGET bob_light)
    add_dependencies(engine_bob_engine_jar bob_light)
  endif()
  install(FILES "${ENGINE_BOB_ENGINE_JAR}" DESTINATION share/java)
  add_subdirectory(src/test)
endif()
