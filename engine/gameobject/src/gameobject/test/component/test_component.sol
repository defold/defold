module test_component

require resource
require io
require ddf
require reflect
require gameobject
require test_resource
require interop

-- This function updates the call counters in the main test .cpp file
!symbol("SolComponentTestIncCounter")
extern test_inc_counter(context:any, comp:String, func:String)


struct Context
    ptr : handle
end

!export("test_comp_alloc_context")
function test_comp_alloc_context():any
    return Context { }
end

struct SharedWorld
    comp_type : String
end

function counter(context:any, world:any, func:String)
    test_inc_counter(context, (world as SharedWorld).comp_type, func)
end

!export("comp_a_new_world")
function comp_a_new_world(params : gameobject.ComponentNewWorldParams) : int
    params.world = SharedWorld {
        comp_type = "a"
    }
    return gameobject.CREATE_RESULT_OK
end

!export("comp_b_new_world")
function comp_b_new_world(params : gameobject.ComponentNewWorldParams) : int
    params.world = SharedWorld {
        comp_type = "b"
    }
    return gameobject.CREATE_RESULT_OK
end

!export("comp_c_new_world")
function comp_c_new_world(params : gameobject.ComponentNewWorldParams) : int
    params.world = SharedWorld {
        comp_type = "c"
    }
    return gameobject.CREATE_RESULT_OK
end

!export("comp_delete_world")
function comp_delete_world(params : gameobject.ComponentDeleteWorldParams) : int
    return gameobject.CREATE_RESULT_OK
end

!export("comp_create")
function comp_create(params : gameobject.ComponentCreateParams) : int
    local k = params.world as SharedWorld
    counter(params.context, params.world, "create")
    return gameobject.CREATE_RESULT_OK
end

!export("comp_destroy")
function comp_destroy(params : gameobject.ComponentDestroyParams) : int
    counter(params.context, params.world, "destroy")
    return gameobject.CREATE_RESULT_OK
end

!export("comp_init")
function comp_init(params : gameobject.ComponentInitParams) : int
    counter(params.context, params.world, "init")
    return gameobject.CREATE_RESULT_OK
end

!export("comp_final")
function comp_final(params : gameobject.ComponentFinalParams) : int
    counter(params.context, params.world, "final")
    return gameobject.CREATE_RESULT_OK
end

!export("comp_add_to_update")
function comp_add_to_update(params : gameobject.ComponentAddToUpdateParams) : int
    counter(params.context, params.world, "add_to_update")
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_update")
function comp_update(params : gameobject.ComponentsUpdateParams) : int
    counter(params.context, params.world, "update")
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_post_update")
function comp_post_update(params : gameobject.ComponentsPostUpdateParams) : int
    counter(params.context, params.world, "post_update")
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_on_message")
function comp_on_message(params : gameobject.ComponentOnMessageParams) : int
    counter(params.context, params.world, "on_message")
    return gameobject.UPDATE_RESULT_OK
end
