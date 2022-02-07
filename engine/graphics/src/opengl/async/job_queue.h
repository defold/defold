// Copyright 2020 The Defold Foundation
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

#ifndef H_GRAPHICS_OPENGL_JOBQUEUE
#define H_GRAPHICS_OPENGL_JOBQUEUE


namespace dmGraphics
{
    struct JobDesc
    {
        void* m_Context;
        void (*m_Func)(void* context);
        void (*m_FuncComplete)(void* context);
    };

    void JobQueueInitialize();
    void JobQueueFinalize();
    bool JobQueueIsAsync();

    void JobQueuePush(const JobDesc& request);
};

#endif // H_GRAPHICS_OPENGL_JOBQUEUE

