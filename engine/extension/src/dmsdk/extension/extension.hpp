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
#include <dmsdk/dlib/log.h>

#if defined(_MSC_VER)
#define DM_EXTENSION_DEPRECATED_CONTEXT_HELPER __declspec(deprecated("Use the context registry API directly instead"))
#elif defined(__GNUC__) || defined(__clang__)
#define DM_EXTENSION_DEPRECATED_CONTEXT_HELPER __attribute__((deprecated("Use the context registry API directly instead")))
#else
#define DM_EXTENSION_DEPRECATED_CONTEXT_HELPER
#endif

namespace dmExtension {

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(HContextRegistry context_registry, const char* name)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ContextRegistryGet directly instead.");
      return (T)ContextRegistryGet(context_registry, name);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsTypeByHash(HContextRegistry context_registry, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsTypeByHash is deprecated. Use ContextRegistryGetByHash directly instead.");
      return (T)ContextRegistryGetByHash(context_registry, name_hash);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(HContextRegistry context_registry, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ContextRegistryGetByHash directly instead.");
      return (T)ContextRegistryGetByHash(context_registry, name_hash);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(dmExtension::AppParams* app_params, const char* name)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ExtensionAppParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGet(ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)app_params), name);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsTypeByHash(dmExtension::AppParams* app_params, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsTypeByHash is deprecated. Use ExtensionAppParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGetByHash(ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)app_params), name_hash);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(dmExtension::AppParams* app_params, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ExtensionAppParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGetByHash(ExtensionAppParamsGetContextRegistry((ExtensionAppParams*)app_params), name_hash);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(dmExtension::Params* params, const char* name)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ExtensionParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGet(ExtensionParamsGetContextRegistry((ExtensionParams*)params), name);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsTypeByHash(dmExtension::Params* params, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsTypeByHash is deprecated. Use ExtensionParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGetByHash(ExtensionParamsGetContextRegistry((ExtensionParams*)params), name_hash);
   }

   template<typename T>
   DM_EXTENSION_DEPRECATED_CONTEXT_HELPER T GetContextAsType(dmExtension::Params* params, dmhash_t name_hash)
   {
      dmLogOnceWarning("%s", "dmExtension::GetContextAsType is deprecated. Use ExtensionParamsGetContextRegistry and the context registry API directly instead.");
      return (T)ContextRegistryGetByHash(ExtensionParamsGetContextRegistry((ExtensionParams*)params), name_hash);
   }

} // namespace

#undef DM_EXTENSION_DEPRECATED_CONTEXT_HELPER

#endif // DMSDK_EXTENSION_HPP
