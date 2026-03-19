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

#include <mbedtls/threading.h>

#if defined (MBEDTLS_THREADING_ALT)
#include <threading_alt.h>
#include <dlib/mutex.h>

void defold_mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex_wrapper)
{
    mutex_wrapper->mutex = dmMutex::New();
}

void defold_mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper->mutex != 0x0)
    {
        dmMutex::Delete((dmMutex::HMutex)mutex_wrapper->mutex);
    }
}

int defold_mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Lock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

int defold_mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Unlock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

#endif
