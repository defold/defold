// Copyright 2020-2023 The Defold Foundation
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

#include "dlib/opaque_handle_container.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

TEST(dmOpaqueHandleContainer, Basic)
{
	dmOpaqueHandleContainer<int> simple;
	ASSERT_EQ(simple.Capacity(), 0);
	ASSERT_TRUE(simple.Full());

	// Container has no storage
	ASSERT_EQ(simple.Put(0), INVALID_OPAQUE_HANDLE);

	const int cap = 4;
	simple.Allocate(cap);
	ASSERT_EQ(simple.Capacity(), cap);
	ASSERT_FALSE(simple.Full());

	int items[cap];
	HOpaqueHandle handles[cap];

	for (int i = 0; i < cap; ++i)
	{
		items[i] = i + 1;
		handles[i] = simple.Put(&items[i]);
	}

	ASSERT_TRUE(simple.Full());

	for (int i = 0; i < cap; ++i)
	{
		ASSERT_NE(handles[i], INVALID_OPAQUE_HANDLE);
		ASSERT_EQ(simple.Get(handles[i]), &items[i]);
	}

	// Test releasing a handle and using that handle to get an object
	simple.Release(handles[0]);
	ASSERT_FALSE(simple.Full());
	ASSERT_EQ((void*) simple.Get(handles[0]), (void*) 0);

	// Test allocating more memory
	simple.Allocate(1);
	ASSERT_FALSE(simple.Full());
}

TEST(dmOpaqueHandleContainer, HandleValidity)
{
	dmOpaqueHandleContainer<int> container(1);

	int value 		= 99;
	HOpaqueHandle h = container.Put(&value);
	ASSERT_EQ(*container.Get(h), value);

	container.Release(h);
	ASSERT_EQ((void*) container.Get(h), (void*) 0);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
