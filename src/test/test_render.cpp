
#include <stdint.h>
#include <gtest/gtest.h>
#include "model/model.h"
#include "render.h"

using namespace Vectormath::Aos;

void SetObjectModel(void* context, void* gameobject, Quat* rotation, Point3* position)
{

}

TEST(dmRender, ModelWorld)
{
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

    for (uint32_t i=0; i<1000; i++)
        dmRender::Update();
//    ASSERT_EQ((uint32_t) 16, a.Capacity());

    for (uint32_t i=0; i<n_ro; i++)
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
