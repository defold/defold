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
        collection = GameObject::NewCollection();
        GameObject::RegisterResourceTypes(factory);
    }

    virtual void TearDown()
    {
        GameObject::DeleteCollection(collection, factory);
        Resource::DeleteFactory(factory);
    }

    GameObject::HCollection collection;
    Resource::HFactory factory;
};

TEST_F(GameObjectTest, Test01)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto01.go");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    GameObject::Delete(collection, factory, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    GameObject::Initialize();
    int ret = RUN_ALL_TESTS();
    GameObject::Finalize();
}
