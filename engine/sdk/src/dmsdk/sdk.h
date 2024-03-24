// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_SDK_H
#define DMSDK_SDK_H

#include <dmsdk/extension/extension.h>
#include <dmsdk/engine/extension.h>
#include <dmsdk/script.h> // references both script/script.h and gamesys/script.h
#include <dmsdk/lua/luaconf.h>
#include <dmsdk/lua/lauxlib.h>
#include <dmsdk/lua/lua.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/condition_variable.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/mutex.h>
// Until we can safely forward declare some Windows.h types, we'll leave this out of the sdk.h
// #include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/vmath.h>

#endif // DMSDK_SDK_H
