module gameobject_private

require gameobject
require vmath
require message
require reflect

let PROPERTY_TYPE_NUMBER:int = 0
let PROPERTY_TYPE_HASH:int = 1
let PROPERTY_TYPE_URL:int = 2
let PROPERTY_TYPE_VECTOR3:int = 3
let PROPERTY_TYPE_VECTOR4:int = 4
let PROPERTY_TYPE_QUAT:int = 5
let PROPERTY_TYPE_BOOLEAN:int = 6
let PROPERTY_TYPE_COUNT:int = 7

-- Return an object that can be used for passing to SetProperty functions in a component
-- For now wrapper objects are used for Number,Hash and Bool, but this should be fixed to
-- use native types, but cannot do until reflect api supports modifying/creating those
!export("gameobject_property_variant_to_any")
function property_variant_to_any(property_type:int):any
    if property_type == PROPERTY_TYPE_NUMBER then
        return 0d
    elseif property_type == PROPERTY_TYPE_HASH then
        return 0u64
    elseif property_type == PROPERTY_TYPE_URL then
        return message.URL { }
    elseif property_type == PROPERTY_TYPE_VECTOR3 then
        return vmath.Vector3 { }
    elseif property_type == PROPERTY_TYPE_VECTOR4 then
        return vmath.Vector4 { }
    elseif property_type == PROPERTY_TYPE_QUAT then
        return vmath.Quat { }
    elseif property_type == PROPERTY_TYPE_BOOLEAN then
        return false -- doh
    else
        return false
    end
end

!export("gameobject_get_variant_type")
function get_variant_type(property:any):int
    local type = reflect.type_of(property)
    if type == reflect.type_of(0d) then
        return PROPERTY_TYPE_NUMBER
    elseif type == reflect.type_of(0u64) then
        return PROPERTY_TYPE_HASH
    elseif type == reflect.type_of(false) then
        return PROPERTY_TYPE_BOOLEAN
    elseif type == reflect.type_of(message.URL { }) then
        return PROPERTY_TYPE_URL
    elseif type == reflect.type_of(vmath.Vector3 { }) then
        return PROPERTY_TYPE_VECTOR3
    elseif type == reflect.type_of(vmath.Vector4 { }) then
        return PROPERTY_TYPE_VECTOR4
    elseif type == reflect.type_of(vmath.Quat { }) then
        return PROPERTY_TYPE_QUAT
    else
        return PROPERTY_TYPE_COUNT
    end
end

------------------------------------------------------
-- Provide structs for interop between c/sol.

struct StaticPool
    dwp  : @gameobject.ComponentDeleteWorldParams
    nwp  : @gameobject.ComponentNewWorldParams
    cp   : @gameobject.ComponentCreateParams
    dp   : @gameobject.ComponentDestroyParams
    ip   : @gameobject.ComponentInitParams
    fp   : @gameobject.ComponentFinalParams
    omp  : @gameobject.ComponentOnMessageParams
    atup : @gameobject.ComponentAddToUpdateParams
    sps  : @gameobject.ComponentSetPropertiesParams
    sp   : @gameobject.ComponentSetPropertyParams
    gp   : @gameobject.ComponentGetPropertyParams
    rp   : @gameobject.ComponentsRenderParams
    up   : @gameobject.ComponentsUpdateParams
    pup  : @gameobject.ComponentsPostUpdateParams
end

local static:StaticPool = StaticPool { }

!export("gameobject_get_comp_new_world_params")
function gameobject_get_comp_new_world_params() : gameobject.ComponentNewWorldParams
    return static.nwp
end

!export("gameobject_get_comp_delete_world_params")
function gameobject_get_comp_delete_world_params() : gameobject.ComponentDeleteWorldParams
    return static.dwp
end

!export("gameobject_get_comp_create_params")
function gameobject_get_comp_create_params() : gameobject.ComponentCreateParams
    return static.cp
end

!export("gameobject_get_comp_destroy_params")
function gameobject_get_comp_destroy_params() : gameobject.ComponentDestroyParams
    return static.dp
end

!export("gameobject_get_comp_init_params")
function gameobject_get_comp_init_params() : gameobject.ComponentInitParams
    return static.ip
end

!export("gameobject_get_comp_final_params")
function gameobject_get_comp_final_params() : gameobject.ComponentFinalParams
    return static.fp
end

!export("gameobject_get_comp_on_message_params")
function gameobject_get_comp_on_message_params() : gameobject.ComponentOnMessageParams
    return static.omp
end

!export("gameobject_get_comp_add_to_update_params")
function gameobject_get_comp_add_to_update_params() : gameobject.ComponentAddToUpdateParams
    return static.atup
end

!export("gameobject_get_comps_update_params")
function gameobject_get_comps_update_params() : gameobject.ComponentsUpdateParams
    return static.up
end

!export("gameobject_get_comps_post_update_params")
function gameobject_get_comps_post_update_params() : gameobject.ComponentsPostUpdateParams
    return static.pup
end

!export("gameobject_get_comps_render_params")
function gameobject_get_comps_render_params() : gameobject.ComponentsRenderParams
    return static.rp
end

!export("gameobject_get_comp_set_properties_params")
function gameobject_get_comp_set_properties_params() : gameobject.ComponentSetPropertiesParams
    return static.sps
end

!export("gameobject_get_comp_set_property_params")
function gameobject_get_comp_set_property_params() : gameobject.ComponentSetPropertyParams
    return static.sp
end

!export("gameobject_get_comp_get_property_params")
function gameobject_get_comp_get_property_params() : gameobject.ComponentGetPropertyParams
    return static.gp
end
