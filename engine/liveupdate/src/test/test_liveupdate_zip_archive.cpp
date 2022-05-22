// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <resource/resource.h>
#include "../liveupdate.h"
#include "../liveupdate_private.h"


static dmResource::HFactory g_Factory = 0;

class LiveUpdate : public jc_test_base_class
{
public:
    virtual void SetUp()
    {
        dmResource::NewFactoryParams factory_params;
        g_Factory = dmResource::NewFactory(&factory_params, ".");
        ASSERT_NE((void*) 0, g_Factory);
        dmLiveUpdate::Initialize(g_Factory);
        dmLiveUpdate::RegisterArchiveLoaders();
    }
    virtual void TearDown()
    {
        dmLiveUpdate::Finalize();
        dmResource::DeleteFactory(g_Factory);
    }
};

TEST(dmLiveUpdate, StoreZipArchiveDoesNotExist)
{
    char* path = "does_not_exist";
    bool verify_archive = true;
    dmLiveUpdate::Result result = dmLiveUpdate::StoreZipArchive(path, verify_archive);
    ASSERT_EQ(dmLiveUpdate::RESULT_INVALID_RESOURCE, result);
}

TEST_F(LiveUpdate, StoreZipArchiveFailedVerification)
{
    char* path = "src/test/data/defold.resourcepack.zip";
    bool verify_archive = true;
    dmLiveUpdate::Result result = dmLiveUpdate::StoreZipArchive(path, verify_archive);
    ASSERT_EQ(dmLiveUpdate::RESULT_INVALID_RESOURCE, result);
}

TEST_F(LiveUpdate, StoreZipArchiveSkipVerification)
{
    char* path = "src/test/data/defold.resourcepack.zip";
    bool verify_archive = false;
    dmLiveUpdate::Result result = dmLiveUpdate::StoreZipArchive(path, verify_archive);
    ASSERT_EQ(dmLiveUpdate::RESULT_OK, result);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
