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

#ifndef DMSDK_EXTENSION_HPP
#define DMSDK_EXTENSION_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif

#include <dmsdk/extension/extension_gen.hpp>

namespace dmExtension {

   template<typename T>
   T GetContextAsType(HContextRegistry context_registry, const char* name)
   {
      return (T)ContextRegistryGetByName(context_registry, name);
   }

   template<typename T>
   T GetContextAsTypeByHash(HContextRegistry context_registry, dmhash_t name_hash)
   {
      return (T)ContextRegistryGetByHash(context_registry, name_hash);
   }

   template<typename T>
   T GetContextAsType(HContextRegistry context_registry, dmhash_t name_hash)
   {
      return GetContextAsTypeByHash<T>(context_registry, name_hash);
   }

   template<typename T>
   T GetContextAsType(dmExtension::AppParams* app_params, const char* name)
   {
      return GetContextAsType<T>(ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)app_params), name);
   }

   template<typename T>
   T GetContextAsTypeByHash(dmExtension::AppParams* app_params, dmhash_t name_hash)
   {
      return GetContextAsTypeByHash<T>(ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)app_params), name_hash);
   }

   template<typename T>
   T GetContextAsType(dmExtension::AppParams* app_params, dmhash_t name_hash)
   {
      return GetContextAsTypeByHash<T>(app_params, name_hash);
   }

   template<typename T>
   T GetContextAsType(dmExtension::Params* params, const char* name)
   {
      return GetContextAsType<T>(ExtensionParamsGetContextRegistry((ExtensionParams*)params), name);
   }

   template<typename T>
   T GetContextAsTypeByHash(dmExtension::Params* params, dmhash_t name_hash)
   {
      return GetContextAsTypeByHash<T>(ExtensionParamsGetContextRegistry((ExtensionParams*)params), name_hash);
   }

   template<typename T>
   T GetContextAsType(dmExtension::Params* params, dmhash_t name_hash)
   {
      return GetContextAsTypeByHash<T>(params, name_hash);
   }


} // namespace

#endif // DMSDK_EXTENSION_HPP
