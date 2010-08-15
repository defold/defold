#ifndef MODEL_H
#define MODEL_H

#include <graphics/material.h>

namespace dmModel
{
    using namespace Vectormath::Aos;


    typedef struct Model* HModel;
    typedef struct Mesh* HMesh;
    typedef class ModelWorld* HWorld;

    /**
     * Create a new model world
     * @param max_models number of models in world
     * @return new model world handle
     */
    HWorld NewWorld(uint32_t max_models);

    /**
     * Destroy model world
     * @param handle to world to destroy
     */
    void DeleteWorld(HWorld);

    /**
     * Update world
     * @param world model world
     */
    void UpdateWorld(HWorld world);

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

    uint32_t GetPrimitiveCount(HMesh mesh);
    dmRender::MeshDesc::Primitive GetPrimitiveType(HMesh mesh);
    const void* GetPositions(HMesh mesh);
    uint32_t    GetPositionCount(HMesh mesh);
    const void* GetTexcoord0(HMesh mesh);
    uint32_t    GetTexcoord0Count(HMesh mesh);
    const void* GetNormals(HMesh mesh);
    uint32_t    GetNormalCount(HMesh mesh);
    const void* GetIndices(HMesh mesh);
    uint32_t    GetIndexCount(HMesh mesh);

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

    /**
     * Add model to world
     * @param world model world
     * @param model model
     */
    void AddModel(HWorld world, HModel model);

}


#endif //__MODEL_H__
