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

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/poolallocator.h"

TEST(dmPoolAllocator, Test)
{
    for (uint32_t page_size = 16; page_size < 1024; page_size += 17)
    {
        dmPoolAllocator::HPool pool = dmPoolAllocator::New(page_size);
        std::map<void*, char> pointer_to_char;
        std::map<void*, uint32_t> pointer_to_size;

        for (uint32_t i = 0; i < 100; ++i)
        {
            uint32_t size = rand() % (page_size + 1);
            char* buffer = (char*) dmPoolAllocator::Alloc(pool, size);
            ASSERT_NE((char*) 0, buffer);
            char c = rand() % 255;
            pointer_to_char[(void*) buffer] = c;
            pointer_to_size[(void*) buffer] = size;
            for (int j = 0; j < (int) size; ++j)
            {
                int val = (c + j) % 255;
                val = c;
                buffer[j] = (char) val;
            }
        }

        std::map<void*, char>::iterator iter = pointer_to_char.begin();
        while (iter != pointer_to_char.end())
        {
            uint32_t size = pointer_to_size[iter->first];

            char c = iter->second;
            char* buffer = (char*) iter->first;
            for (int j = 0; j < (int) size; ++j)
            {
                int val = (c + j) % 255;
                val = c;
                ASSERT_EQ((int) val, (int) buffer[j]);
            }

            ++iter;
        }

        dmPoolAllocator::Delete(pool);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

