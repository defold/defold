
#include <stdint.h>
#include <gtest/gtest.h>
#include <ddf/ddf.h>
#include <graphics/graphics_device.h>
#include "../../build/default/proto/render/mesh_ddf.h"
#include "../../build/default/proto/render/material_ddf.h"
#include "render.h"
#include "../model/model.h"


using namespace Vectormath::Aos;

void SetObjectModel(void* context, void* gameobject, Quat* rotation, Point3* position)
{

}



TEST(dmRenderTest, InitializeFinalize)
{
    return;
    const uint32_t n_models = 1000;
    const uint32_t n_ro = 100;

    dmRender::Initialize(1000, 10, SetObjectModel);


    dmModel::HWorld world = dmModel::NewWorld(n_models);

    dmModel::HModel model[n_models];

    for (uint32_t i=0; i<n_models; i++)
    {
        model[i] = dmModel::NewModel();
        dmModel::AddModel(world, model[i]);
    }


    dmRender::HRenderObject ro[n_ro];
    for (uint32_t i=0; i<n_ro; i++)
    {
        ro[i] = NewRenderObjectInstance((void*)model[i], (void*)0x0, dmRender::RENDEROBJECT_TYPE_PARTICLE);
    }

    for (uint32_t i=0; i<n_ro; i++)
    {
        dmRender::Update();
        if (i & 1 == 0)
            DeleteRenderObject(ro[i]);
    }
//    ASSERT_EQ((uint32_t) 16, a.Capacity());

    for (uint32_t i=0; i<n_ro; i++)
        if (i & 1 == 1)
            DeleteRenderObject(ro[i]);


    for (uint32_t i=0; i<n_models; i++)
        dmModel::DeleteModel(model[i]);

    dmModel::DeleteWorld(world);
    dmRender::Finalize();
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
