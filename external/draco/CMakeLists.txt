cmake_minimum_required(VERSION 4.0)

set(DRACO_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/draco-1.5.7/src")
# Match upstream DRACO_GLTF_BITSTREAM: mesh, normals, standard edgebreaker.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/draco/draco_features.h"
  "#ifndef DRACO_FEATURES_H_\n#define DRACO_FEATURES_H_\n"
  "#define DRACO_MESH_COMPRESSION_SUPPORTED\n"
  "#define DRACO_NORMAL_ENCODING_SUPPORTED\n"
  "#define DRACO_STANDARD_EDGEBREAKER_SUPPORTED\n#endif\n")

# Decoder source groups from the pinned upstream CMakeLists.txt, excluding
# encoders, point-cloud KD trees, IO, plugins, and transcoder dependencies.
set(DRACO_DECODER_SOURCES
  attributes/attribute_octahedron_transform.cc
  attributes/attribute_quantization_transform.cc
  attributes/attribute_transform.cc
  attributes/geometry_attribute.cc
  attributes/point_attribute.cc
  compression/attributes/attributes_decoder.cc
  compression/attributes/sequential_attribute_decoder.cc
  compression/attributes/sequential_attribute_decoders_controller.cc
  compression/attributes/sequential_integer_attribute_decoder.cc
  compression/attributes/sequential_normal_attribute_decoder.cc
  compression/attributes/sequential_quantization_attribute_decoder.cc
  compression/bit_coders/adaptive_rans_bit_decoder.cc
  compression/bit_coders/direct_bit_decoder.cc
  compression/bit_coders/rans_bit_decoder.cc
  compression/bit_coders/symbol_bit_decoder.cc
  compression/decode.cc
  compression/draco_compression_options.cc
  compression/entropy/symbol_decoding.cc
  compression/mesh/mesh_decoder.cc
  compression/mesh/mesh_edgebreaker_decoder.cc
  compression/mesh/mesh_edgebreaker_decoder_impl.cc
  compression/mesh/mesh_sequential_decoder.cc
  compression/point_cloud/point_cloud_decoder.cc
  compression/point_cloud/point_cloud_sequential_decoder.cc
  core/bit_utils.cc
  core/bounding_box.cc
  core/data_buffer.cc
  core/decoder_buffer.cc
  core/divide.cc
  core/draco_types.cc
  core/encoder_buffer.cc
  core/hash_utils.cc
  core/options.cc
  core/quantization_utils.cc
  mesh/corner_table.cc
  mesh/mesh.cc
  mesh/mesh_attribute_corner_table.cc
  mesh/mesh_misc_functions.cc
  mesh/mesh_stripifier.cc
  metadata/geometry_metadata.cc
  metadata/metadata.cc
  metadata/metadata_decoder.cc
  point_cloud/point_cloud.cc
)
list(TRANSFORM DRACO_DECODER_SOURCES PREPEND "${DRACO_SOURCE_DIR}/draco/")
add_library(draco_decoder STATIC ${DRACO_DECODER_SOURCES})
target_include_directories(draco_decoder PRIVATE "${DRACO_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(draco_decoder PRIVATE cxx_std_11)
set_target_properties(draco_decoder PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Match the CRT/STL used by the tools in Debug builds too.
target_compile_definitions(draco_decoder PRIVATE $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

defold_install_targets(TARGETS draco_decoder ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${DRACO_SOURCE_DIR}/draco" DESTINATION include FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/draco/draco_features.h" DESTINATION include/draco)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/package/draco-1.5.7/LICENSE" DESTINATION share/licenses/draco)
