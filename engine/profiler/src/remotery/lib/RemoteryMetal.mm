//
// Copyright 2014-2018 Celtoys Ltd
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <Foundation/NSThread.h>
#include <Foundation/NSDictionary.h>
#include <Foundation/NSString.h>

#import <Metal/Metal.h>

// Store command buffer in thread-local so that each thread can point to its own
static void SetCommandBuffer(id command_buffer)
{
    NSMutableDictionary* thread_data = [[NSThread currentThread] threadDictionary];
    thread_data[@"rmtMTLCommandBuffer"] = command_buffer;
}

static id GetCommandBuffer()
{
    NSMutableDictionary* thread_data = [[NSThread currentThread] threadDictionary];
    return thread_data[@"rmtMTLCommandBuffer"];
}

extern "C" void _rmt_BindMetal(id command_buffer)
{
    SetCommandBuffer(command_buffer);
}

extern "C" void _rmt_UnbindMetal()
{
    SetCommandBuffer(0);
}

// Needs to be in the same lib for this to work
extern "C" unsigned long long rmtMetal_usGetTime();

static void SetTimestamp(void* data)
{
    *((unsigned long long*)data) = rmtMetal_usGetTime();
}

extern "C" void rmtMetal_MeasureCommandBuffer(unsigned long long* out_start, unsigned long long* out_end, unsigned int* out_ready)
{
    id command_buffer = GetCommandBuffer();
    [command_buffer addScheduledHandler:^(id <MTLCommandBuffer>){ SetTimestamp(out_start); }];
    [command_buffer addCompletedHandler:^(id <MTLCommandBuffer>){ SetTimestamp(out_end); *out_ready = 1; }];
}
