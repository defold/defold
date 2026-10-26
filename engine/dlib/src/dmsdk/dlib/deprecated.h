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

#ifndef DMSDK_DEPRECATED_H
#define DMSDK_DEPRECATED_H

/*# Deprecated API annotation
 *
 * Compiler-independent annotation for deprecated APIs.
 *
 * @document
 * @name Deprecated
 * @language C
 */

/*# deprecated API annotation
 * Marks an API as deprecated and emits a compiler warning when it is used.
 *
 * @macro
 * @name DM_DEPRECATED
 * @param message [type:string] deprecation message
 */
#if defined(_MSC_VER)
#define DM_DEPRECATED(message) __declspec(deprecated(message))
#elif defined(__GNUC__) || defined(__clang__)
#define DM_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#define DM_DEPRECATED(message)
#endif

#endif // DMSDK_DEPRECATED_H
