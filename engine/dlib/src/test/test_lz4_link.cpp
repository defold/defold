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

#include <dlib/lz4.h>
#include <dmsdk/dlib/shared_library.h>

// Retain the engine's decompression path without referencing compression.
extern "C" DM_DLLEXPORT int Lz4DecodeOnly(const void* source, uint32_t size, void* output, uint32_t capacity, int* decoded_size)
{
    return dmLZ4::DecompressBuffer(source, size, output, capacity, decoded_size);
}
