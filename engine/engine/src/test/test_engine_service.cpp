// Copyright 2020-2026 The Defold Foundation
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

#include "test_engine.h"
#include "../engine_service_private.h"

TEST_F(EngineTest, ServiceInstanceNameSanitizesAddress)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("Local Host", "8001", 0, out, sizeof(out));
    ASSERT_STREQ("defold-local-host-8001", out);
}

TEST_F(EngineTest, SanitizeMDNSLabelReplacesLeadingDashWhenRequested)
{
    char out[64];
    dmEngineService::SanitizeMDNSLabel("-service-id", "defold", true, 'd', out, sizeof(out));
    ASSERT_STREQ("dservice-id", out);
}

TEST_F(EngineTest, ManufacturerModelNameOmitsDanglingSeparator)
{
    char out[64];
    dmEngineService::BuildManufacturerModelName("Acme", 0, out, sizeof(out));
    ASSERT_STREQ("Acme", out);

    dmEngineService::BuildManufacturerModelName(0, "Device", out, sizeof(out));
    ASSERT_STREQ("Device", out);
}

TEST_F(EngineTest, ServiceInstanceNameDropsAddressBeforeDanglingSeparator)
{
    char out[12];
    dmEngineService::BuildServiceInstanceName("very-long-local-address", "8001", 0, out, sizeof(out));
    ASSERT_STREQ("defold-8001", out);
}

TEST_F(EngineTest, ServiceInstanceNameAvoidsTrailingDashWithoutPort)
{
    char out[8];
    dmEngineService::BuildServiceInstanceName("very-long-local-address", 0, 0, out, sizeof(out));
    ASSERT_STREQ("defold", out);
}

TEST_F(EngineTest, ServiceInstanceNameIncludesSuffix)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("Local Host", "8001", "89abcdef", out, sizeof(out));
    ASSERT_STREQ("defold-local-host-8001-89abcdef", out);
}

TEST_F(EngineTest, ServiceInstanceNamePreservesSuffixWhenTruncatingAddress)
{
    char out[64];
    dmEngineService::BuildServiceInstanceName("very-long-local-address-name-for-discovery-target-node", "8001", "89abcdef", out, sizeof(out));
    ASSERT_STREQ("defold-very-long-local-address-name-for-discovery-8001-89abcdef", out);
}

TEST_F(EngineTest, DiscoveryIdentityUsesRoutableAddress)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("fe80::1", "editor-host", "Acme", "Device", "Linux", out, sizeof(out));
    ASSERT_STREQ("fe80::1", out);
}

TEST_F(EngineTest, DiscoveryIdentityFallsBackToHostnameForLoopback)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("127.0.0.1", "editor-host", "Acme", "Device", "Linux", out, sizeof(out));
    ASSERT_STREQ("editor-host", out);
}

TEST_F(EngineTest, DiscoveryIdentityFallsBackToManufacturerAndModel)
{
    char out[128];
    dmEngineService::BuildDiscoveryIdentity("localhost", "localhost", "Acme", "Device", "Android", out, sizeof(out));
    ASSERT_STREQ("Acme-Device", out);
}
