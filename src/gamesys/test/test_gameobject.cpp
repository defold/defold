#include <gtest/gtest.h>

#include <dlib/hash.h>
#include "../resource.h"
#include "../gameobject.h"

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        factory = Resource::NewFactory(16, "build/default/src/gamesys/test");
        GameObject::RegisterResourceTypes(factory);
    }

    virtual void TearDown()
    {
        Resource::DeleteFactory(factory);
    }

    Resource::HFactory factory;
};

TEST_F(GameObjectTest, Test01)
{
    GameObject::HInstance go = GameObject::New(factory, "goproto01.go");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = GameObject::Update(go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(go);
    ASSERT_TRUE(ret);
    GameObject::Delete(factory, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    GameObject::Initialize();
    int ret = RUN_ALL_TESTS();
    GameObject::Finalize();
}
