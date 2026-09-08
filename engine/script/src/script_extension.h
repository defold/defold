// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#ifndef DM_SCRIPT_EXTENSION_H
#define DM_SCRIPT_EXTENSION_H

struct lua_State;

namespace dmScript
{
    typedef struct Context*         HContext;
    typedef struct ScriptWorld*     HScriptWorld;
    typedef struct ScriptExtension* HScriptExtension;

    /**
     * Use a ScriptExtension to hook into various callbacks of the script lifetime
     *
     * For callbacks you do not care for, set them to 0x0
     */
    struct ScriptExtension
    {
        // Called when the context has completed Initialize(HContext context)
        void (*Initialize)(HContext context);
        // Called one each game frame
        void (*Update)(HContext context);
        // Called just before the context completes Finalize(HContext context)
        void (*Finalize)(HContext context);
        // Called when a new "world" has been created (a Collection, GUI Scene etc)
        void (*NewScriptWorld)(HScriptWorld script_world);
        // Called just before deleting the script world
        void (*DeleteScriptWorld)(HScriptWorld script_world);
        // Called once a frame for the world, dt is local time delta (affected by slo-mo etc)
        void (*UpdateScriptWorld)(HScriptWorld script_world, float dt);
        // Called once a frame for the world, dt is local time delta (affected by slo-mo etc)
        void (*FixedUpdateScriptWorld)(HScriptWorld script_world, float dt);
        // Called when a script instance has been created
        void (*InitializeScriptInstance)(HScriptWorld script_world);
        // Called just before a script instance is deleted
        void (*FinalizeScriptInstance)(HScriptWorld script_world);
        // Called inside the protected-call error handler, before stack unwinding.
        // Error value is at index 1. Must preserve the stack and must not raise.
        void (*OnError)(HContext context, lua_State* L);
    };

    HContext   GetScriptContext(lua_State* L);
    lua_State* GetLuaState(HContext context);
    void       RegisterScriptExtension(HContext context, HScriptExtension script_extension);
} // namespace dmScript
#endif
