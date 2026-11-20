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

#ifndef DM_DLIB_JOBSYSTEM_H
#define DM_DLIB_JOBSYSTEM_H

#include <dmsdk/dlib/jobsystem.h>

// Called under the queue mutex. Inspect the borrowed job/context without modifying
// jobs or calling the job system; neither pointer may be retained.
typedef bool (*FJobFilter)(const Job* job, void* context);

// Process at most one ready job accepted by filter on the calling thread.
// filter_context is borrowed for this call. Callbacks remain queued for JobSystemUpdate.
// If wait is true, wait for work or completion when nothing is ready. The context owner
// must have pending jobs; other users' jobs may wake it without satisfying the filter.
void JobSystemProcessOne(HJobContext context, bool wait, FJobFilter filter, void* filter_context);

void JobSystemDebugPrintJobs(HJobContext context);

#endif // DM_DLIB_JOBSYSTEM_H
