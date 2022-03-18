// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_SHARED_LIBRARY_H
#define DMSDK_SHARED_LIBRARY_H


/*# SDK Shared library API documentation
 *
 * Utility functions for shared library export/import
 *
 * @document
 * @name Shared Library
 * @namespace sharedlibrary
 * @path engine/dlib/src/dmsdk/dlib/shared_library.h
 */

/*# storage-class attribute for shared library export/import
 *
 * Export and import functions, data, and objects to or from a DLL
 *
 * @macro
 * @name DM_DLLEXPORT
 * @examples
 *
 * ```cpp
 * DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);
 * ```
 *
 */
#ifdef _MSC_VER
#define DM_DLLEXPORT __declspec(dllexport)
#else
#define DM_DLLEXPORT __attribute__ ((visibility ("default")))
#endif

#endif // DMSDK_SHARED_LIBRARY_H
