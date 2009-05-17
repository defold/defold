#include <gtest/gtest.h>

#include <dlib/hash.h>
#include "../resource.h"
#include "../gameobject.h"

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        factory = Resource::NewFactory(16, ".");
        GameObject::RegisterResourceTypes(factory);
    }

    virtual void TearDown()
    {
        Resource::DeleteFactory(factory);
    }

    Resource::HFactory factory;
};

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
