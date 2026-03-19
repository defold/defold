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

#ifndef DM_DEFOLD_MBEDTLS_THRADING_H
#define DM_DEFOLD_MBEDTLS_THRADING_H

#include "threading_alt.h"

#if defined (MBEDTLS_THREADING_ALT)
extern void defold_mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex_wrapper);
extern void defold_mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex_wrapper);
extern int defold_mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex_wrapper);
extern int defold_mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex_wrapper);

#endif

#endif
