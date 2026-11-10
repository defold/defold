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

#include <dmsdk/datastore/datastore.h>

// Only the engine's lifecycle and scheduling entry points remain when the
// datastore is excluded. No context, allocations or jobs are created.
HDSContext DSStoreCreate(const DSCreateParams*)
{
    return 0;
}

DSResult DSStoreDestroy(HDSContext)
{
    return DS_RESULT_OK;
}

DSResult DSEventAdd(HDSContext, const DSEventDesc*)
{
    return DS_RESULT_NOT_FOUND;
}

DSResult DSSchedulerBegin(HDSContext, HJobContext)
{
    return DS_RESULT_NOT_FOUND;
}

DSResult DSSchedulerEvent(HDSContext, dmhash_t)
{
    return DS_RESULT_NOT_FOUND;
}

DSResult DSSchedulerSync(HDSContext)
{
    return DS_RESULT_NOT_FOUND;
}

DSResult DSSchedulerEnd(HDSContext)
{
    return DS_RESULT_NOT_FOUND;
}
