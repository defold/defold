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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/context_registry.h>

TEST(ContextRegistry, SetAndGet)
{
    HContextRegistry registry = ContextRegistryCreate();

    int alpha = 1;
    int beta = 2;

    ASSERT_EQ(0, ContextRegistrySet(registry, "missing", 0));
    ASSERT_EQ(0, ContextRegistryGet(registry, "missing"));

    ASSERT_EQ(0, ContextRegistrySet(registry, "alpha", &alpha));
    ASSERT_EQ(&alpha, ContextRegistryGet(registry, "alpha"));
    ASSERT_EQ(&alpha, ContextRegistryGetByHash(registry, dmHashString64("alpha")));

    ASSERT_EQ(0, ContextRegistrySetByHash(registry, dmHashString64("beta"), &beta));
    ASSERT_EQ(&beta, ContextRegistryGet(registry, "beta"));
    ASSERT_EQ(&beta, ContextRegistryGetByHash(registry, dmHashString64("beta")));

    ASSERT_EQ(0, ContextRegistrySet(registry, "alpha", 0));
    ASSERT_EQ(0, ContextRegistryGet(registry, "alpha"));

    ASSERT_EQ(0, ContextRegistrySetByHash(registry, dmHashString64("beta"), 0));
    ASSERT_EQ(0, ContextRegistryGetByHash(registry, dmHashString64("beta")));

    ContextRegistryDestroy(registry);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
