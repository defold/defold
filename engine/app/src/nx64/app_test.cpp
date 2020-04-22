#include <assert.h>
#include <stdio.h>
#include "app_test.h"

#include <nn/fs.h>
#include <nn/fs/fs_Debug.h>
namespace dmApp
{

#if defined(DM_APP_TEST_MODE)
    void TestInitialize()
    {
        nn::fs::MountSaveDataForDebug("save"); // using this instead of relying on the user account selection
        nn::fs::MountCacheStorage("cache");
        nn::fs::MountHost("host", ".");
    }

    void TestFinalize()
    {
        nn::fs::Unmount("save");
        nn::fs::Unmount("cache");
        nn::fs::Unmount("host");
    }
#else
    void TestInitialize() {}
    void TestFinalize() {}
#endif
}
