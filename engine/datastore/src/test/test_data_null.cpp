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
#include <dmsdk/datastore/datastore.h>

// Excluding the datastore must provide the engine's link symbols without
// creating a store or allowing lifecycle events and schedules to run.
TEST(DataNull, EngineEntryPoints)
{
    HDSContext store = DSStoreCreate(0);
    ASSERT_EQ((HDSContext)0, store);
    DSCreateParams params = {};
    ASSERT_EQ((HDSContext)0, DSStoreCreate(&params));
    DSEventDesc event = { 1 };
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSEventAdd(store, &event));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerBegin(store, 0));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerEvent(store, event.m_Name));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerSync(store));
    ASSERT_EQ(DS_RESULT_NOT_FOUND, DSSchedulerEnd(store));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
