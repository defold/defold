# Portable render functionality shared by Bob, the editor and the engine.
# Keep this target independent of graphics backends and the renderer itself.
find_package(JNI REQUIRED)

set(RENDER_SHARED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(RENDER_SHARED_MATERIAL_PROTO "${RENDER_SHARED_GEN_DIR}/render/material_ddf.cpp")
set(RENDER_SHARED_GRAPHICS_PROTO "${RENDER_SHARED_GEN_DIR}/graphics/graphics_ddf.cpp")
defold_protoc_gen_cpp("${RENDER_SHARED_MATERIAL_PROTO}"
  "${RENDER_PROTO_ROOT}/render/material_ddf.proto"
  INCLUDES "${RENDER_PROTO_ROOT}" "${DEFOLD_HOME}/engine/graphics/proto")
defold_protoc_gen_cpp("${RENDER_SHARED_GRAPHICS_PROTO}"
  "${DEFOLD_HOME}/engine/graphics/proto/graphics/graphics_ddf.proto"
  INCLUDES "${DEFOLD_HOME}/engine/graphics/proto")
add_custom_target(render_shared_proto
  DEPENDS "${RENDER_SHARED_MATERIAL_PROTO}" "${RENDER_SHARED_GRAPHICS_PROTO}")

add_library(render_shared SHARED
  "${RENDER_SRC_DIR}/render/material_render_state.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/render_jni.cpp"
  "${RENDER_SHARED_MATERIAL_PROTO}"
  "${RENDER_SHARED_GRAPHICS_PROTO}")
defold_target_skip_sanitizer(render_shared)
target_include_directories(render_shared PRIVATE
  "${RENDER_SHARED_GEN_DIR}" "${RENDER_SRC_DIR}"
  "${DEFOLD_HOME}/engine/graphics/src" ${JNI_INCLUDE_DIRS})
add_dependencies(render_shared render_shared_proto)
defold_target_link_libraries(render_shared "${TARGET_PLATFORM}"
  dlib_noasan ddf_noasan jni_noasan profile_null_noasan)
defold_target_link_app(render_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)
defold_codesign_target(render_shared "${DEFOLD_HOME}/engine/engine/darwin/entitlements-macos-debug.plist")
defold_install_targets(TARGETS render_shared
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION lib/${TARGET_PLATFORM})

set(RENDER_JAR "${CMAKE_CURRENT_BINARY_DIR}/render.jar")
set(RENDER_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
set(RENDER_JAVA_SOURCE "${RENDER_SRC_DIR}/java/com/dynamo/bob/pipeline/RenderJni.java")
add_custom_command(
  OUTPUT "${RENDER_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${RENDER_CLASSES_DIR}"
  COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
    -d "${RENDER_CLASSES_DIR}" "${RENDER_JAVA_SOURCE}"
  COMMAND "${Java_JAR_EXECUTABLE}" cf "${RENDER_JAR}" -C "${RENDER_CLASSES_DIR}" .
  DEPENDS "${RENDER_JAVA_SOURCE}"
  COMMENT "Packaging render.jar")
add_custom_target(render_jar ALL DEPENDS "${RENDER_JAR}")
install(FILES "${RENDER_JAR}" DESTINATION share/java)
