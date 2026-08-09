// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#ifndef DM_GAMESYS_SCRIPT_BULLET3D_H
#define DM_GAMESYS_SCRIPT_BULLET3D_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>

#include <btBulletDynamicsCommon.h>

struct lua_State;

namespace dmGameObject
{
    typedef struct CollectionHandle* HCollection;
}

namespace dmGameSystem
{
    void                      SetBullet3DPhysicsScale(float scale);
    float                     GetBullet3DPhysicsScale();
    float                     GetBullet3DInvPhysicsScale();

    btVector3                 CheckBullet3DVector3(lua_State* L, int index, float scale);
    btQuaternion              CheckBullet3DQuat(lua_State* L, int index);
    void                      PushBullet3DVector3(lua_State* L, const btVector3& value, float scale);
    void                      PushBullet3DQuat(lua_State* L, const btQuaternion& value);

    btDiscreteDynamicsWorld*  CheckBullet3DWorld(lua_State* L, int index);
    btDiscreteDynamicsWorld*  ToBullet3DWorld(lua_State* L, int index);
    bool                      IsBullet3DWorldValid(lua_State* L, int index);
    void                      PushBullet3DWorld(lua_State* L, void* world, void* component_world);
    void                      ScriptBullet3DInvalidateWorld(void* world);
    void                      ScriptBullet3DInitializeWorld(lua_State* L);
    void                      ScriptBullet3DFinalizeWorld();

    btCollisionObject*        CheckBullet3DCollisionObject(lua_State* L, int index);
    btCollisionObject*        ToBullet3DCollisionObject(lua_State* L, int index);
    bool                      IsBullet3DCollisionObjectValid(lua_State* L, int index);
    dmGameObject::HCollection GetBullet3DCollisionObjectCollection(lua_State* L, int index);
    void                      PushBullet3DCollisionObject(lua_State* L, void* collision_object, dmGameObject::HCollection collection, dmhash_t instance_id);
    void                      ScriptBullet3DInvalidateCollisionObject(void* collision_object);
    void                      ScriptBullet3DInitializeCollisionObject(lua_State* L);
    void                      ScriptBullet3DFinalizeCollisionObject();

    btRigidBody*              CheckBullet3DRigidBody(lua_State* L, int index);
    void                      ScriptBullet3DInitializeRigidBody(lua_State* L);
    void                      ScriptBullet3DFinalizeRigidBody();
} // namespace dmGameSystem

#endif // DM_GAMESYS_SCRIPT_BULLET3D_H
