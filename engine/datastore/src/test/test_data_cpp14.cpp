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

// Compiled separately as strict C++14 to check the public inline implementations.
#include <dmsdk/datastore/datastore.h>

#if (defined(_MSVC_LANG) && _MSVC_LANG != 201402L) || (!defined(_MSVC_LANG) && __cplusplus != 201402L)
#error The public header consumer must compile as C++14
#endif
