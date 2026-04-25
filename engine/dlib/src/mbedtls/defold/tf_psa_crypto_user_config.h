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

#ifndef DM_TF_PSA_CRYPTO_USER_CONFIG_H
#define DM_TF_PSA_CRYPTO_USER_CONFIG_H

#if defined(DM_MBEDTLS_DISABLE_ASM)
#undef MBEDTLS_HAVE_ASM
#endif

#if defined(DM_MBEDTLS_CRYPTO_ONLY)
#undef PSA_WANT_ALG_RIPEMD160
#undef PSA_WANT_ALG_SHA_224
#undef PSA_WANT_ALG_SHA_384
#undef PSA_WANT_ALG_SHA3_224
#undef PSA_WANT_ALG_SHA3_256
#undef PSA_WANT_ALG_SHA3_384
#undef PSA_WANT_ALG_SHA3_512
#endif

#endif
