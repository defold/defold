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


#ifndef DM_TESTMAIN_H
#define DM_TESTMAIN_H

#include <stdint.h>

extern "C" bool TestMainPlatformInit();
extern "C" int  TestMainIsDebuggerAttached();

/*# Do a http get
 * @param url [type: const char*]
 * @param length [type: uint32_t*] (mandatory)
 * @param content [type: const char*] (optional). If 0, no content is loaded. allocated memory must be deallocated usin free()
 * @return status http result code (200 OK)
 */
int TestHttpGet(const char* url, uint32_t* length, const char** content);

#endif // DM_TESTMAIN_H
