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
#include <stdio.h>
#include <stdlib.h>

uint8_t* ReadFixtureBlob(const char* name, uint32_t* out_size)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", DATA_BENCHMARK_FIXTURE_DIR, name);
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Cannot open fixture blob: %s\n", path);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    *out_size = (uint32_t)ftell(file);
    rewind(file);
    uint8_t* bytes = new uint8_t[*out_size];
    if (fread(bytes, 1, *out_size, file) != *out_size)
    {
        fprintf(stderr, "Cannot read fixture blob: %s\n", path);
        fclose(file);
        delete[] bytes;
        exit(1);
    }
    fclose(file);
    return bytes;
}
