#ifndef MODEL_H
#define MODEL_H

#include <graphics/material.h>

namespace dmModel
{
    using namespace Vectormath::Aos;

    enum PrimtiveType
    {
        TRIANGLES = 1,
        QUADS     = 2,
    };

    typedef struct Model* HModel;
    typedef struct Mesh* HMesh;
    typedef class ModelWorld* HWorld;

    /**
     * Create a new model
     * @return new model handle
     */
    HModel NewModel();

    /**
     * Destroy a model
     * @param world model world
     * @param model model to destroy
     */
    void DeleteModel(HModel model);

    /**
     * Create a new mesh
     * @return New mesh handle
     */
    HMesh NewMesh();

    /**
     * Destroy a mesh
     * @param mesh mesh to destroy
     */
    void DeleteMesh(HMesh mesh);

    /**
     * Get number of primitives (triangles/quads) in mesh
     * @param mesh Mesh to query
     * @return Number of primitives
     */
    uint32_t        GetPrimitiveCount(HMesh mesh);

    /**
     * Get primitive type for mesh
     * @param mesh Mesh to query
     * @return Primitive type
     */
    PrimtiveType    GetPrimitiveType(HMesh mesh);

    /**
     * Get vertex positions for mesh
     * @param mesh Mesh to query
     * @return Pointer to vertex data
     */
    const void*     GetPositions(HMesh mesh);

    /**
     * Get vertex position count
     * @param mesh Mesh to query
     * @return Vertex data count
     */
    uint32_t        GetPositionCount(HMesh mesh);

    /**
     * Get texture0 coords for mesh
     * @param mesh Mesh to query
     * @return Pointer to texture coord data
     */
    const void*     GetTexcoord0(HMesh mesh);

    /**
     * Get texture0 coord count
     * @param mesh Mesh to query
     * @return Texture0 coord count
     */
    uint32_t        GetTexcoord0Count(HMesh mesh);

    /**
     * Get normals for mesh
     * @param mesh Mesh to query
     * @return Pointer to normal data
     */
    const void*     GetNormals(HMesh mesh);

    /**
     * Get normal count
     * @param mesh Mesh to query
     * @return Normal count
     */
    uint32_t        GetNormalCount(HMesh mesh);

    /**
     * Get index data for mesh
     * @param mesh Mesh to query
     * @return Pointer to index data
     */
    const void*     GetIndices(HMesh mesh);

    /**
     * Get index count
     * @param mesh Mesh to query
     * @return Index count
     */
    uint32_t        GetIndexCount(HMesh mesh);

    /**
     * Set model mesh
     * @param model model
     * @param mesh mesh
     */
    void SetMesh(HModel model, HMesh mesh);

    /**
     * Set model texture0
     * @param model model
     * @param texture texture0
     */
    void SetTexture0(HModel model, dmGraphics::HTexture texture);

    /**
     * Set model material
     * @param model model
     * @param material material
     */
    void SetMaterial(HModel model, dmGraphics::HMaterial material);

    /**
     * Get mesh from model
     * @param model Model
     * @return Mesh associated with model
     */
    HMesh GetMesh(HModel model);

    /**
     * Get texture0 from model
     * @param model Model
     * @return Texture0 associated with model
     */
    dmGraphics::HTexture GetTexture0(HModel model);

    /**
     * Get material from model
     * @param model Model
     * @return Material associated with model
     */
    dmGraphics::HMaterial GetMaterial(HModel model);

}


#endif //MODEL_H
