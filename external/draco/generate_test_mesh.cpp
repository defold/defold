// Fixture generator only; compiled against the pinned upstream Draco encoder.
#include <draco/compression/encode.h>
#include <draco/compression/decode.h>
#include <draco/mesh/mesh_stripifier.h>
#include <draco/mesh/mesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3)
        return 1;
    float       offset = (float)atof(argv[2]);
    draco::Mesh mesh;
    mesh.set_num_points(4);
    const float              position[4][3] = { { offset, 0, 0 }, { offset + 1, 0, 0 }, { offset, 1, 0 }, { offset + 1, 1, 0 } };
    draco::GeometryAttribute attribute;
    attribute.Init(draco::GeometryAttribute::POSITION, 0, 3, draco::DT_FLOAT32, false, 12, 0);
    int id = mesh.AddAttribute(attribute, true, 4);
    mesh.attribute(id)->set_unique_id(42);
    for (int i = 0; i < 4; ++i)
        mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), position[i]);
    const uint8_t color[4] = { 255, 128, 0, 255 };
    attribute.Init(draco::GeometryAttribute::COLOR, 0, 4, draco::DT_UINT8, true, 4, 0);
    id = mesh.AddAttribute(attribute, false, 1);
    mesh.attribute(id)->set_unique_id(77);
    mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(0), color);
    for (int i = 0; i < 4; ++i)
        mesh.attribute(id)->SetPointMapEntry(draco::PointIndex(i), draco::AttributeValueIndex(0));
    const uint8_t joints[4] = { 0, 0, 0, 0 };
    attribute.Init(draco::GeometryAttribute::GENERIC, 0, 4, draco::DT_UINT8, false, 4, 0);
    id = mesh.AddAttribute(attribute, false, 1);
    mesh.attribute(id)->set_unique_id(101);
    mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(0), joints);
    for (int i = 0; i < 4; ++i)
        mesh.attribute(id)->SetPointMapEntry(draco::PointIndex(i), draco::AttributeValueIndex(0));
    const uint8_t weights[4] = { 255, 0, 0, 0 };
    attribute.Init(draco::GeometryAttribute::GENERIC, 0, 4, draco::DT_UINT8, true, 4, 0);
    id = mesh.AddAttribute(attribute, false, 1);
    mesh.attribute(id)->set_unique_id(205);
    mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(0), weights);
    for (int i = 0; i < 4; ++i)
        mesh.attribute(id)->SetPointMapEntry(draco::PointIndex(i), draco::AttributeValueIndex(0));
    draco::Mesh::Face face;
    face[0] = 0;
    face[1] = 1;
    face[2] = 2;
    mesh.AddFace(face);
    face[0] = 2;
    face[1] = 1;
    face[2] = 3;
    mesh.AddFace(face);
    draco::Encoder encoder;
    encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
    draco::EncoderBuffer encoded;
    if (!encoder.EncodeMeshToBuffer(mesh, &encoded).ok())
        return 2;
    FILE* file = fopen(argv[1], "wb");
    if (!file)
        return 3;
    fwrite(encoded.data(), 1, encoded.size(), file);
    fclose(file);
    draco::DecoderBuffer input;
    input.Init(encoded.data(), encoded.size());
    draco::Decoder decoder;
    draco::Mesh    decoded;
    if (!decoder.DecodeBufferToGeometry(&input, &decoded).ok())
        return 4;
    std::vector<uint32_t> strip;
    draco::MeshStripifier stripifier;
    if (!stripifier.GenerateTriangleStripsWithDegenerateTriangles(decoded, std::back_inserter(strip)))
        return 5;
    printf("vertices=%u strip=%zu\n", decoded.num_points(), strip.size());
    return 0;
}
