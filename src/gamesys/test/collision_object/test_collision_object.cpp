#include <gtest/gtest.h>

#include <resource/resource.h>

#include <sound/sound.h>
#include <gameobject/gameobject.h>

#include "gamesys/gamesys.h"
#include "../test_gamesys.h"

class CollisionObjectTest : public GamesysTest
{
};

TEST_F(CollisionObjectTest, TestFailingInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "illegal_mass.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
