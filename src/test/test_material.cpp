#include <stdint.h>
#include <gtest/gtest.h>
#include "render/material.h"

TEST(dmMaterialTest, CreateDestroy)
{
    dmRender::HMaterial material = dmRender::NewMaterial();
    dmRender::DeleteMaterial(material);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
