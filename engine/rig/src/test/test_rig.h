#ifndef TESTRIG_H
#define TESTRIG_H

#include <rig/rig.h>

namespace dmRig
{
    static void CreateDummyMeshEntry(dmRigDDF::MeshEntry& mesh_entry, dmhash_t id, Vector4 color) 
    {
        mesh_entry.m_Id = id;
        mesh_entry.m_Meshes.m_Data = new dmRigDDF::Mesh[1];
        mesh_entry.m_Meshes.m_Count = 1;

        uint32_t vert_count = 3;

        // set vertice position so they match bone positions
        dmRigDDF::Mesh& mesh = mesh_entry.m_Meshes.m_Data[0];
        mesh.m_Positions.m_Data = new float[vert_count*3];
        mesh.m_Positions.m_Count = vert_count*3;
        mesh.m_Positions.m_Data[0] = 0.0f;
        mesh.m_Positions.m_Data[1] = 0.0f;
        mesh.m_Positions.m_Data[2] = 0.0f;
        mesh.m_Positions.m_Data[3] = 1.0f;
        mesh.m_Positions.m_Data[4] = 0.0f;
        mesh.m_Positions.m_Data[5] = 0.0f;
        mesh.m_Positions.m_Data[6] = 2.0f;
        mesh.m_Positions.m_Data[7] = 0.0f;
        mesh.m_Positions.m_Data[8] = 0.0f;

        // data for each vertex (tex coords and normals not used)
        mesh.m_Texcoord0.m_Data       = new float[vert_count*2];
        mesh.m_Texcoord0.m_Count      = vert_count*2;
        mesh.m_Normals.m_Data         = new float[vert_count*3];
        mesh.m_Normals.m_Count        = vert_count*3;

        mesh.m_Color.m_Data           = new float[vert_count*4];
        mesh.m_Color.m_Count          = vert_count*4;
        mesh.m_Color[0]               = color.getX();
        mesh.m_Color[1]               = color.getY();
        mesh.m_Color[2]               = color.getZ();
        mesh.m_Color[3]               = color.getW();
        mesh.m_Color[4]               = color.getX();
        mesh.m_Color[5]               = color.getY();
        mesh.m_Color[6]               = color.getZ();
        mesh.m_Color[7]               = color.getW();
        mesh.m_Color[8]               = color.getX();
        mesh.m_Color[9]               = color.getY();
        mesh.m_Color[10]              = color.getZ();
        mesh.m_Color[11]              = color.getW();
        mesh.m_Indices.m_Data         = new uint32_t[vert_count];
        mesh.m_Indices.m_Count        = vert_count;
        mesh.m_Indices.m_Data[0]      = 0;
        mesh.m_Indices.m_Data[1]      = 1;
        mesh.m_Indices.m_Data[2]      = 2;
        mesh.m_BoneIndices.m_Data     = new uint32_t[vert_count*4];
        mesh.m_BoneIndices.m_Count    = vert_count*4;
        mesh.m_BoneIndices.m_Data[0]  = 0;
        mesh.m_BoneIndices.m_Data[1]  = 1;
        mesh.m_BoneIndices.m_Data[2]  = 0;
        mesh.m_BoneIndices.m_Data[3]  = 0;
        mesh.m_BoneIndices.m_Data[4]  = 0;
        mesh.m_BoneIndices.m_Data[5]  = 1;
        mesh.m_BoneIndices.m_Data[6]  = 0;
        mesh.m_BoneIndices.m_Data[7]  = 0;
        mesh.m_BoneIndices.m_Data[8]  = 0;
        mesh.m_BoneIndices.m_Data[9]  = 1;
        mesh.m_BoneIndices.m_Data[10] = 0;
        mesh.m_BoneIndices.m_Data[11] = 0;

        mesh.m_Weights.m_Data         = new float[vert_count*4];
        mesh.m_Weights.m_Count        = vert_count*4;
        mesh.m_Weights.m_Data[0]      = 1.0f;
        mesh.m_Weights.m_Data[1]      = 0.0f;
        mesh.m_Weights.m_Data[2]      = 0.0f;
        mesh.m_Weights.m_Data[3]      = 0.0f;
        mesh.m_Weights.m_Data[4]      = 0.0f;
        mesh.m_Weights.m_Data[5]      = 1.0f;
        mesh.m_Weights.m_Data[6]      = 0.0f;
        mesh.m_Weights.m_Data[7]      = 0.0f;
        mesh.m_Weights.m_Data[8]      = 0.0f;
        mesh.m_Weights.m_Data[9]      = 1.0f;
        mesh.m_Weights.m_Data[10]     = 0.0f;
        mesh.m_Weights.m_Data[11]     = 0.0f;

        mesh.m_Visible = true;
        mesh.m_DrawOrder = 0;
    }
}

#endif // TESTRIG_H