// Copyright 2020-2025 The Defold Foundation
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

#include "gameobject.h"
#include "component.h"
#include "comp_anim.h"
#include "comp_script.h"
#include <dlib/log.h>

namespace dmGameObject
{
    static Result CompScriptcInit(const ComponentTypeCreateCtx* ctx, ComponentType* type)
    {
        ComponentTypeSetPrio(type, 200);
        ComponentTypeSetContext(type, ctx->m_Script);
        ComponentTypeSetHasUserData(type, true);
        ComponentTypeSetReadsTransforms(type, true);

        ComponentTypeSetNewWorldFn(type, CompScriptNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompScriptDeleteWorld);
        ComponentTypeSetCreateFn(type, CompScriptCreate);
        ComponentTypeSetDestroyFn(type, CompScriptDestroy);
        ComponentTypeSetInitFn(type, CompScriptInit);
        ComponentTypeSetFinalFn(type, CompScriptFinal);
        ComponentTypeSetAddToUpdateFn(type, CompScriptAddToUpdate);
        ComponentTypeSetPreUpdateFn(type, CompScriptPreUpdate);
        ComponentTypeSetPreFixedUpdateFn(type, CompScriptPreFixedUpdate);
        ComponentTypeSetLateUpdateFn(type, CompScriptLateUpdate);
        ComponentTypeSetOnMessageFn(type, CompScriptOnMessage);
        ComponentTypeSetOnInputFn(type, CompScriptOnInput);
        ComponentTypeSetOnReloadFn(type, CompScriptOnReload);
        ComponentTypeSetSetPropertiesFn(type, CompScriptSetProperties);
        ComponentTypeSetGetPropertyFn(type, CompScriptGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompScriptSetProperty);
        ComponentTypeSetPropertyIteratorFn(type, CompScriptIterProperties);

        return dmGameObject::RESULT_OK;
    }

    static Result CompAnimcInit(const ComponentTypeCreateCtx* ctx, ComponentType* type)
    {
        ComponentTypeSetPrio(type, 250);
        ComponentTypeSetReadsTransforms(type, true);
        ComponentTypeSetNewWorldFn(type, CompAnimNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompAnimDeleteWorld);
        ComponentTypeSetAddToUpdateFn(type, CompAnimAddToUpdate);
        ComponentTypeSetPreUpdateFn(type, CompAnimPreUpdate);
        return dmGameObject::RESULT_OK;
    }

}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeAnim, "animc", dmGameObject::CompAnimcInit, 0);
DM_DECLARE_COMPONENT_TYPE(ComponentTypeScript, "scriptc", dmGameObject::CompScriptcInit, 0);
