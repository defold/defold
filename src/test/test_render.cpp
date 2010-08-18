
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
    const uint32_t n_models = 1000;
    const uint32_t n_ro = 100;


    dmModel::HModel model[n_models];

    for (uint32_t i=0; i<n_models; i++)
    {
        model[i] = dmModel::NewModel();
    }

    dmRender::HRenderWorld world = dmRender::NewRenderWorld(n_ro, 10, SetObjectModel);

    dmRender::HRenderObject ro[n_ro];
    for (uint32_t i=0; i<n_ro; i++)
    {
        ro[i] = NewRenderObjectInstance(world, (void*)model[i], (void*)0x0, dmRender::RENDEROBJECT_TYPE_PARTICLE);
    }

    for (uint32_t i=0; i<n_ro; i++)
    {
        dmRender::Update(world, 0.0167);
        if (i & 1 == 0)
            DeleteRenderObject(world, ro[i]);
    }
//    ASSERT_EQ((uint32_t) 16, a.Capacity());

    for (uint32_t i=0; i<n_ro; i++)
        if (i & 1 == 1)
            DeleteRenderObject(world, ro[i]);


    for (uint32_t i=0; i<n_models; i++)
        dmModel::DeleteModel(model[i]);

    dmRender::DeleteRenderWorld(world);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
