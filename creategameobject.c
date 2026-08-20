// Standalone helper for creating a .goc resource with an in-place mesh component.

#include <stdio.h>
#include <string.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/resource/resource.h>
#include <gameobject/gameobject_ddf.h>
#include <gamesys/buffer_ddf.h>
#include <gamesys/mesh_ddf.h>

ResourceResult CreateGameObjectResourceWithMesh(HResourceFactory factory, const char* path, const char* material_path, dmGameObject::HPrototype* out_resource)
{
    char buffer_path[512];
    char mesh_path[512];
    snprintf(buffer_path, sizeof(buffer_path), "%s.bufferc", path);
    snprintf(mesh_path, sizeof(mesh_path), "%s.meshc", path);

    float position_data[3] = {0.0f, 0.0f, 0.0f};
    float normal_data[3] = {0.0f, 0.0f, 0.0f};
    dmBufferDDF::StreamDesc stream_descs[2] = {};
    stream_descs[0].m_Name = "position";
    stream_descs[0].m_NameHash = dmHashString64("position");
    stream_descs[0].m_ValueType = dmBufferDDF::VALUE_TYPE_FLOAT32;
    stream_descs[0].m_ValueCount = 3;
    stream_descs[0].m_F.m_Data = position_data;
    stream_descs[0].m_F.m_Count = 3;
    stream_descs[1].m_Name = "normal";
    stream_descs[1].m_NameHash = dmHashString64("normal");
    stream_descs[1].m_ValueType = dmBufferDDF::VALUE_TYPE_FLOAT32;
    stream_descs[1].m_ValueCount = 3;
    stream_descs[1].m_F.m_Data = normal_data;
    stream_descs[1].m_F.m_Count = 3;
    dmBufferDDF::BufferDesc buffer_desc = {};
    buffer_desc.m_Streams.m_Data = stream_descs;
    buffer_desc.m_Streams.m_Count = 2;
    dmArray<uint8_t> buffer_ddf;
    dmDDF::SaveMessageToArray(&buffer_desc, dmBufferDDF::BufferDesc::m_DDFDescriptor, buffer_ddf);
    void* buffer_resource = 0;
    ResourceCreateResource(factory, buffer_path, buffer_ddf.Begin(), buffer_ddf.Size(), &buffer_resource);

    dmMeshDDF::MeshDesc mesh_desc = {};
    mesh_desc.m_Material = material_path;
    mesh_desc.m_Vertices = buffer_path;
    mesh_desc.m_PrimitiveType = dmMeshDDF::MeshDesc::PRIMITIVE_TRIANGLES;
    mesh_desc.m_PositionStream = "position";
    mesh_desc.m_NormalStream = "normal";
    dmArray<uint8_t> mesh_ddf;
    dmDDF::SaveMessageToArray(&mesh_desc, dmMeshDDF::MeshDesc::m_DDFDescriptor, mesh_ddf);
    void* mesh_resource = 0;
    ResourceCreateResource(factory, mesh_path, mesh_ddf.Begin(), mesh_ddf.Size(), &mesh_resource);

    dmGameObjectDDF::ComponentDesc component_desc = {};
    component_desc.m_Id = "mesh";
    component_desc.m_Component = mesh_path;
    dmGameObjectDDF::PrototypeDesc prototype_desc = {};
    prototype_desc.m_Components.m_Data = &component_desc;
    prototype_desc.m_Components.m_Count = 1;
    dmArray<uint8_t> ddf_buffer;
    dmDDF::SaveMessageToArray(&prototype_desc, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, ddf_buffer);
    return ResourceCreateResource(factory, path, ddf_buffer.Begin(), ddf_buffer.Size(), (void**)out_resource);
}
