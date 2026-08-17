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

#include <stdio.h>

#include <dlib/log.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "gamesys_private.h"

#include "components/box2d/comp_collision_object_box2d.h"

#include <extension/extension.hpp>

#include "script_box2d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    //////////////////////////////////////////////////////////////////////////////

    static float g_PhysicsScale = 1.0f;
    static float g_InvPhysicsScale = 1.0f / g_PhysicsScale;

    void SetPhysicsScale(float scale)
    {
        g_PhysicsScale = scale;
        g_InvPhysicsScale = 1.0f / g_PhysicsScale;
    }

    float GetPhysicsScale()
    {
        return g_PhysicsScale;
    }

    float GetInvPhysicsScale()
    {
        return g_InvPhysicsScale;
    }

    //////////////////////////////////////////////////////////////////////////////

    static void GetCollisionObject(lua_State* L, int index, dmGameObject::HCollection collection, dmMessage::URL* url, dmGameObject::HComponent* comp, void** comp_world)
    {
        dmGameObject::GetComponentFromLua(L, index, collection, COLLISION_OBJECT_EXT, comp, url, comp_world);
    }

    static int B2D_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        void* comp_world = dmGameObject::GetWorld(collection, component_type_index);
        void* world = dmGameSystem::CompCollisionObjectGetBox2DWorld(comp_world);

        if (world)
            PushWorld(L, world);
        else
            lua_pushnil(L);
        return 1;
    }

    static int B2D_GetBody(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        dmMessage::URL url;
        dmGameObject::HComponent component = 0;
        GetCollisionObject(L, 1, collection, &url, &component, 0);

        void* body = dmGameSystem::CompCollisionObjectGetBox2DBody(component);

        if (body)
        {
            PushBody(L, body, collection, url.m_Path);
        }
        else
            lua_pushnil(L);
        return 1;
    }

    static int B2D_GetVersion(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBox2DVersion(L);
        return 1;
    }

    static const luaL_reg BOX2D_FUNCTIONS[] =
    {
        {"get_world", B2D_GetWorld},
        {"get_body", B2D_GetBody},
        {"get_version", B2D_GetVersion},

        {0, 0}
    };

    static dmExtension::Result ScriptBox2DInitialize(dmExtension::Params* params)
    {
        float physics_scale_default = 1.0f;
        float physics_scale = params->m_ConfigFile ? dmConfigFile::GetFloat(params->m_ConfigFile, "physics.scale", physics_scale_default) : physics_scale_default;
        dmGameSystem::SetPhysicsScale(physics_scale);

        lua_State* L = params->m_L;
        luaL_register(L, "b2d", dmGameSystem::BOX2D_FUNCTIONS);

        dmGameSystem::ScriptBox2DInitializeBody(L);
        dmGameSystem::CompCollisionObjectSetBox2DInvalidateBodyCallback(dmGameSystem::ScriptBox2DInvalidateBody);

        lua_pop(L, 1); // pop the lua module
        return dmExtension::RESULT_OK;
    }


    static dmExtension::Result ScriptBox2DFinalize(dmExtension::Params* params)
    {
        dmGameSystem::CompCollisionObjectSetBox2DInvalidateBodyCallback(0);
        dmGameSystem::ScriptBox2DFinalizeBody();
        return dmExtension::RESULT_OK;
    }


    DM_DECLARE_EXTENSION(ScriptBox2DExt, "ScriptBox2d", 0, 0, ScriptBox2DInitialize, 0, 0, ScriptBox2DFinalize)

}

/*# Box2D documentation
 *
 * Functions for interacting with Box2D.
 *
 * @document
 * @name b2d
 * @namespace b2d
 * @language Lua
 */


/*# Box2D world
 *
 * An opaque handle to the Box2D physics world owned by the current collection.
 * Obtain it with [ref:b2d.get_world] and pass it to functions in `b2d.world`.
 * The engine creates and destroys the world together with the collection; it
 * cannot be constructed directly from Lua.
 *
 * @typedef
 * @name b2World
 * @param value [type:userdata] Box2D world handle
 * @examples
 *
 * ```lua
 * local world = b2d.get_world()
 * if world then
 *     pprint(world)
 * end
 * ```
 */

/*# Box2D body
 *
 * An opaque handle to the native Box2D body of a collision-object component.
 * Obtain it with [ref:b2d.get_body] and pass it to functions in `b2d.body`.
 * The collision object owns the body, so the handle becomes invalid when its
 * component or game object is deleted.
 *
 * @typedef
 * @name b2Body
 * @param value [type:userdata] Box2D body handle
 * @examples
 *
 * ```lua
 * local body = b2d.get_body("#collisionobject")
 * if body then
 *     print(b2d.body.get_position(body))
 * end
 * ```
 */

/*# Box2D body contact entry
 *
 * An opaque entry in the contact list of a Box2D body. Obtain the first entry
 * with [ref:b2d.body.get_contact_list]. Contact entries are owned by the physics
 * world and must not be created directly.
 *
 * @typedef
 * @name b2ContactEdge
 * @param value [type:userdata] body contact-list entry
 * @examples
 *
 * ```lua
 * local body = b2d.get_body("#collisionobject")
 * local first_contact = b2d.body.get_contact_list(body)
 * if first_contact then
 *     pprint(first_contact)
 * end
 * ```
 */

/*# Box2D mass data
 *
 * Mass properties for a Box2D body or shape.
 *
 * @struct
 * @name b2d.mass_data
 * @member mass [type:number] Body mass, usually in kilograms.
 * @member center [type:vector3] Local center of mass.
 * @member inertia [type:number] Rotational inertia about the local origin.
 */

/*# Box2D transform
 *
 * World transform for a Box2D body.
 *
 * @struct
 * @name b2d.transform
 * @member position [type:vector3] World position of the body origin.
 * @member angle [type:number] World rotation angle in radians.
 */

/*# Box2D version information
 * @struct
 * @name b2d.version_info
 * @member version [type:string] Full Box2D version string.
 * @member major [type:integer] Major version number.
 * @member middle [type:integer] Middle version number.
 * @member minor [type:integer] Minor version number.
 */

/*# Box2D collision filter
 * @struct
 * @name b2d.filter
 * @member category_bits [type:integer] Collision category bits.
 * @member mask_bits [type:integer] Collision mask bits.
 * @member group_index [type:integer] Collision group index.
 */

/*# Partial Box2D collision filter
 * @struct
 * @name b2d.filter_options
 * @member category_bits? [type:integer] Collision category bits.
 * @member mask_bits? [type:integer] Collision mask bits.
 * @member group_index? [type:integer] Collision group index.
 */

/*# Box2D world-query filter
 * @struct
 * @name b2d.query_filter
 * @member category_bits? [type:integer] Optional collision category bits.
 * @member mask_bits? [type:integer] Optional collision mask bits.
 * @member group_index? [type:integer] Optional collision group index. Supported by the Box2D 2.x backend.
 */

/*# Box2D axis-aligned bounding box
 * @struct
 * @name b2d.aabb
 * @member lower [type:vector3] Lower bound.
 * @member upper [type:vector3] Upper bound.
 */

/*# Box2D broad-phase query statistics
 * @struct
 * @name b2d.tree_stats
 * @member node_visits [type:integer] Number of tree nodes visited.
 * @member leaf_visits [type:integer] Number of tree leaves visited.
 */

/*# Box2D 2.x fixture information
 * @struct
 * @name b2d.fixture_info
 * @member body? [type:b2Body] Owning body, when returned from a world query.
 * @member index [type:integer] Fixture index on the body.
 * @member child_index? [type:integer] Child-shape index, when returned from a world query.
 * @member type [type:b2d.shape.SHAPE_TYPE] Shape type.
 * @member sensor [type:boolean] Whether the fixture is a sensor.
 * @member density [type:number] Fixture density.
 * @member friction [type:number] Fixture friction.
 * @member restitution [type:number] Fixture restitution.
 * @member child_count [type:integer] Number of child shapes.
 */

/*# Box2D 3.x shape information
 * @struct
 * @name b2d.shape_info
 * @member index [type:integer] Shape index on the body.
 * @member shape_id [type:b2Shape] Shape handle.
 * @member type [type:b2d.shape.SHAPE_TYPE] Shape type.
 * @member sensor [type:boolean] Whether the shape is a sensor.
 * @member density [type:number] Shape density.
 * @member friction [type:number] Shape friction.
 * @member restitution [type:number] Shape restitution.
 * @member material [type:integer] Shape material identifier.
 * @member child_count [type:integer] Number of child shapes.
 * @member is_chain_segment [type:boolean] Whether the shape belongs to a chain.
 */

/*# Box2D 2.x fixture definition
 * @struct
 * @name b2d.fixture_definition
 * @member shape [type:b2d.shape.definition] Shape definition.
 * @member friction? [type:number] Fixture friction.
 * @member restitution? [type:number] Fixture restitution.
 * @member density? [type:number] Fixture density.
 * @member sensor? [type:boolean] Whether the fixture is a sensor.
 * @member is_sensor? [type:boolean] Alias for `sensor`.
 * @member filter? [type:b2d.filter] Collision filter.
 */

/*# Box2D 3.x shape creation definition
 *
 * Geometry and material properties accepted by [ref:b2d.body.create_shape]. The
 * geometry can be supplied in the `shape` field as a [type:b2d.shape.definition],
 * or its fields can be placed directly in this table. Material properties such
 * as `density`, `friction`, `restitution`, and `filter` are optional.
 *
 * @typedef
 * @name b2d.shape_create_definition
 * @param value [type:{ shape:b2d.shape.definition, density?:number, friction?:number, restitution?:number, material?:integer, sensor?:boolean, is_sensor?:boolean, filter?:b2d.filter }|{ type:b2d.shape.SHAPE_TYPE, radius?:number, center?:vector3, center1?:vector3, center2?:vector3, v0?:vector3, v1?:vector3, v2?:vector3, v3?:vector3, hx?:number, hy?:number, angle?:number, vertices?:vector3[], density?:number, friction?:number, restitution?:number, material?:integer, sensor?:boolean, is_sensor?:boolean, filter?:b2d.filter }]
 * @examples
 *
 * Create a circular shape using an inline geometry definition:
 *
 * ```lua
 * local body = b2d.get_body("#collisionobject")
 * local shape = b2d.body.create_shape(body, {
 *     type = b2d.shape.SHAPE_TYPE_CIRCLE,
 *     radius = 16,
 *     density = 1,
 *     friction = 0.4,
 * })
 * ```
 */

/*# Box2D 3.x chain definition
 * @struct
 * @name b2d.chain_definition
 * @member vertices [type:vector3[]] Chain vertices.
 * @member loop? [type:boolean] Whether the chain is closed.
 * @member prev_vertex? [type:vector3] Ghost vertex preceding an open chain.
 * @member next_vertex? [type:vector3] Ghost vertex following an open chain.
 * @member friction? [type:number] Segment friction.
 * @member restitution? [type:number] Segment restitution.
 * @member material? [type:integer] Segment material identifier.
 * @member filter? [type:b2d.filter_options] Collision filter fields to override.
 * @member enable_sensor_events? [type:boolean] Whether to enable sensor events.
 */

/*# Box2D chain geometry
 * @struct
 * @name b2d.chain_geometry
 * @member loop [type:boolean] Whether the chain is closed.
 * @member segment_count [type:integer] Number of chain segments.
 * @member vertices [type:vector3[]] Chain vertices.
 * @member prev_vertex? [type:vector3] Ghost vertex preceding an open chain.
 * @member next_vertex? [type:vector3] Ghost vertex following an open chain.
 */

/*# Box2D 2.x cast hit
 * @struct
 * @name b2d.fixture_cast_hit
 * @member fixture [type:b2d.fixture_info] Hit fixture.
 * @member shape [type:b2d.fixture_info] Hit fixture child shape.
 * @member point [type:vector3] Hit point.
 * @member normal [type:vector3] Hit normal.
 * @member fraction [type:number] Hit fraction.
 * @member node_visits? [type:integer] Number of tree nodes visited by a closest query.
 * @member leaf_visits? [type:integer] Number of tree leaves visited by a closest query.
 */

/*# Box2D 3.x cast hit
 * @struct
 * @name b2d.shape_cast_hit
 * @member shape [type:b2d.shape_info] Hit shape.
 * @member point [type:vector3] Hit point.
 * @member normal [type:vector3] Hit normal.
 * @member fraction [type:number] Hit fraction.
 * @member node_visits? [type:integer] Number of tree nodes visited by a closest query.
 * @member leaf_visits? [type:integer] Number of tree leaves visited by a closest query.
 */

/*# Direct Box2D shape cast result
 * @struct
 * @name b2d.shape_cast_output
 * @member point [type:vector3] Hit point.
 * @member normal [type:vector3] Hit normal.
 * @member fraction [type:number] Hit fraction.
 * @member iterations [type:integer] Number of cast iterations.
 */

/*# Box2D contact manifold point
 * @struct
 * @name b2d.contact_point
 * @member point [type:vector3] World contact point.
 * @member anchor_a [type:vector3] Contact anchor on the first body.
 * @member anchor_b [type:vector3] Contact anchor on the second body.
 * @member separation [type:number] Contact separation.
 * @member normal_impulse [type:number] Normal impulse.
 * @member tangent_impulse [type:number] Tangent impulse.
 * @member total_normal_impulse [type:number] Total normal impulse.
 * @member normal_velocity [type:number] Relative normal velocity.
 * @member id [type:integer] Contact point identifier.
 * @member persisted [type:boolean] Whether the point persisted from the previous step.
 */

/*# Box2D contact data
 * @struct
 * @name b2d.contact_data
 * @member shape_a [type:b2d.shape_info] First contact shape.
 * @member shape_b [type:b2d.shape_info] Second contact shape.
 * @member normal [type:vector3] Contact normal.
 * @member rolling_impulse [type:number] Rolling resistance impulse.
 * @member point_count [type:integer] Number of manifold points.
 * @member points [type:b2d.contact_point[]] Contact manifold points.
 */

/*# Box2D mover capsule
 * @struct
 * @name b2d.mover_capsule
 * @member center1 [type:vector3] First capsule center.
 * @member center2 [type:vector3] Second capsule center.
 * @member radius [type:number] Capsule radius.
 */

/*# Box2D mover collision plane
 * @struct
 * @name b2d.mover_plane
 * @member shape [type:b2d.shape_info] Colliding shape.
 * @member normal [type:vector3] Plane normal.
 * @member offset [type:number] Plane offset.
 * @member hit [type:boolean] Whether the mover hit the plane.
 */

/*# Box2D explosion definition
 * @struct
 * @name b2d.explosion_definition
 * @member position [type:vector3] Explosion center.
 * @member radius [type:number] Explosion radius.
 * @member falloff [type:number] Distance over which the impulse falls off.
 * @member impulse_per_length [type:number] Impulse applied per unit length.
 * @member mask_bits? [type:integer] Optional collision mask.
 */

/*# Box2D world profiling data
 * @struct
 * @name b2d.world_profile
 * @member step [type:number] Total step time.
 * @member pairs [type:number] Pair update time.
 * @member collide [type:number] Collision time.
 * @member solve [type:number] Solver time.
 * @member merge_islands [type:number] Island merge time.
 * @member prepare_stages [type:number] Stage preparation time.
 * @member solve_constraints [type:number] Constraint solver time.
 * @member prepare_constraints [type:number] Constraint preparation time.
 * @member integrate_velocities [type:number] Velocity integration time.
 * @member warm_start [type:number] Warm-start time.
 * @member solve_impulses [type:number] Impulse solver time.
 * @member integrate_positions [type:number] Position integration time.
 * @member relax_impulses [type:number] Impulse relaxation time.
 * @member apply_restitution [type:number] Restitution time.
 * @member store_impulses [type:number] Impulse storage time.
 * @member split_islands [type:number] Island splitting time.
 * @member transforms [type:number] Transform update time.
 * @member hit_events [type:number] Hit-event generation time.
 * @member refit [type:number] Tree refit time.
 * @member bullets [type:number] Bullet processing time.
 * @member sleep_islands [type:number] Island sleeping time.
 * @member sensors [type:number] Sensor processing time.
 */

/*# Box2D world counters
 * @struct
 * @name b2d.world_counters
 * @member body_count [type:integer] Number of bodies.
 * @member shape_count [type:integer] Number of shapes.
 * @member contact_count [type:integer] Number of contacts.
 * @member joint_count [type:integer] Number of joints.
 * @member island_count [type:integer] Number of islands.
 * @member stack_used [type:integer] Stack bytes in use.
 * @member static_tree_height [type:integer] Static broad-phase tree height.
 * @member tree_height [type:integer] Dynamic broad-phase tree height.
 * @member byte_count [type:integer] Allocated byte count.
 * @member task_count [type:integer] Number of tasks.
 * @member color_counts [type:integer[]] Constraint graph color counts.
 */

/*# Box2D distance-joint definition
 * @struct
 * @name b2d.joint.distance_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member length? [type:number] Rest length.
 * @member min_length? [type:number] Minimum length.
 * @member max_length? [type:number] Maximum length.
 * @member enable_spring? [type:boolean] Whether the spring is enabled.
 * @member hertz? [type:number] Spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency alias.
 * @member damping_ratio? [type:number] Spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member enable_limit? [type:boolean] Whether length limits are enabled.
 * @member enable_motor? [type:boolean] Whether the motor is enabled.
 * @member max_motor_force? [type:number] Maximum motor force.
 * @member motor_speed? [type:number] Motor speed.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D mouse-joint definition
 * @struct
 * @name b2d.joint.mouse_definition
 * @member target? [type:vector3] Target position.
 * @member max_force? [type:number] Maximum force.
 * @member hertz? [type:number] Spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency alias.
 * @member damping_ratio? [type:number] Spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D prismatic-joint definition
 * @struct
 * @name b2d.joint.prismatic_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member local_axis_a? [type:vector3] Local translation axis on the first body.
 * @member reference_angle? [type:number] Reference angle.
 * @member enable_spring? [type:boolean] Whether the spring is enabled.
 * @member hertz? [type:number] Spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency alias.
 * @member damping_ratio? [type:number] Spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member enable_limit? [type:boolean] Whether translation limits are enabled.
 * @member lower_translation? [type:number] Lower translation limit.
 * @member upper_translation? [type:number] Upper translation limit.
 * @member enable_motor? [type:boolean] Whether the motor is enabled.
 * @member max_motor_force? [type:number] Maximum motor force.
 * @member motor_speed? [type:number] Motor speed.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D revolute-joint definition
 * @struct
 * @name b2d.joint.revolute_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member reference_angle? [type:number] Reference angle.
 * @member enable_spring? [type:boolean] Whether the spring is enabled.
 * @member hertz? [type:number] Spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency alias.
 * @member damping_ratio? [type:number] Spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member enable_limit? [type:boolean] Whether angular limits are enabled.
 * @member lower_angle? [type:number] Lower angular limit.
 * @member upper_angle? [type:number] Upper angular limit.
 * @member enable_motor? [type:boolean] Whether the motor is enabled.
 * @member max_motor_torque? [type:number] Maximum motor torque.
 * @member motor_speed? [type:number] Motor speed.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D weld-joint definition
 * @struct
 * @name b2d.joint.weld_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member reference_angle? [type:number] Reference angle.
 * @member hertz? [type:number] Legacy spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency.
 * @member damping_ratio? [type:number] Legacy spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member linear_hertz? [type:number] Linear spring frequency in hertz.
 * @member angular_hertz? [type:number] Angular spring frequency in hertz.
 * @member linear_damping_ratio? [type:number] Linear damping ratio.
 * @member angular_damping_ratio? [type:number] Angular damping ratio.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D wheel-joint definition
 * @struct
 * @name b2d.joint.wheel_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member local_axis_a? [type:vector3] Local suspension axis on the first body.
 * @member enable_spring? [type:boolean] Whether the spring is enabled.
 * @member hertz? [type:number] Spring frequency in hertz.
 * @member frequency? [type:number] Legacy spring frequency alias.
 * @member damping_ratio? [type:number] Spring damping ratio.
 * @member damping? [type:number] Legacy spring damping-ratio alias.
 * @member enable_limit? [type:boolean] Whether translation limits are enabled.
 * @member lower_translation? [type:number] Lower translation limit.
 * @member upper_translation? [type:number] Upper translation limit.
 * @member enable_motor? [type:boolean] Whether the motor is enabled.
 * @member max_motor_torque? [type:number] Maximum motor torque.
 * @member motor_speed? [type:number] Motor speed.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D friction-joint definition
 * @struct
 * @name b2d.joint.friction_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member max_force? [type:number] Maximum friction force.
 * @member max_torque? [type:number] Maximum friction torque.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D rope-joint definition
 * @struct
 * @name b2d.joint.rope_definition
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member max_length? [type:number] Maximum rope length.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D pulley-joint definition
 * @struct
 * @name b2d.joint.pulley_definition
 * @member ground_anchor_a? [type:vector3] First ground anchor.
 * @member ground_anchor_b? [type:vector3] Second ground anchor.
 * @member local_anchor_a? [type:vector3] Local anchor on the first body.
 * @member local_anchor_b? [type:vector3] Local anchor on the second body.
 * @member length_a? [type:number] First segment length.
 * @member length_b? [type:number] Second segment length.
 * @member ratio? [type:number] Pulley ratio.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D gear-joint definition
 * @struct
 * @name b2d.joint.gear_definition
 * @member ratio? [type:number] Gear ratio.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D motor-joint definition
 * @struct
 * @name b2d.joint.motor_definition
 * @member linear_offset? [type:vector3] Linear target offset.
 * @member angular_offset? [type:number] Angular target offset.
 * @member max_force? [type:number] Maximum motor force.
 * @member max_torque? [type:number] Maximum motor torque.
 * @member correction_factor? [type:number] Position correction factor.
 * @member collide_connected? [type:boolean] Whether connected bodies collide.
 */

/*# Box2D filter-joint definition
 *
 * The optional definition passed to [ref:b2d.joint.create_filter]. Filter joints
 * currently have no configurable fields, so omit the argument or pass an empty
 * table. The type is reserved for future options.
 *
 * @typedef
 * @name b2d.joint.filter_definition
 * @param value [type:{}] empty filter-joint options
 * @examples
 *
 * ```lua
 * local body_a = b2d.get_body("#collisionobject_a")
 * local body_b = b2d.get_body("#collisionobject_b")
 * local joint = b2d.joint.create_filter(body_a, body_b, {})
 * ```
 */

/*# Get the Box2D world from the current collection
 * @name b2d.get_world
 * @return world [type: b2World] the world if successful. Otherwise `nil`.
 */

/*# Get the Box2D body from a collision object
 * @name b2d.get_body
 * @param url [type: string|hash|url] the url to the game object collision component
 * @return body [type: b2Body] the body if successful. Otherwise `nil`.
 */

/*# Get the Box2D version information for the active backend.
 * @name b2d.get_version
 * @return info [type:b2d.version_info] version info with fields `version`, `major`, `middle`, and `minor`
 */
