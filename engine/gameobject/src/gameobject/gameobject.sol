module gameobject

require ddf
require vmath
require message
require interop

let CREATE_RESULT_OK:int = 0
let CREATE_RESULT_UNKNOWN_ERROR:int = -1000

let UPDATE_RESULT_OK:int = 0
let UPDATE_RESULT_UNKNOWN_ERROR:int = -1000

let PROPERTY_RESULT_OK:int = 0
let PROPERTY_RESULT_NOT_FOUND:int = -1
let PROPERTY_RESULT_INVALID_FORMAT:int = -2
let PROPERTY_RESULT_UNSUPPORTED_TYPE:int = -3
let PROPERTY_RESULT_TYPE_MISMATCH:int = -4
let PROPERTY_RESULT_COMP_NOT_FOUND:int = -5
let PROPERTY_RESULT_INVALID_INSTANCE:int =-6
let PROPERTY_RESULT_BUFFER_OVERFLOW:int = -7
let PROPERTY_RESULT_UNSUPPORTED_VALUE:int = -8
let PROPERTY_RESULT_UNSUPPORTED_OPERATION:int = -9

struct ComponentNewWorldParams
    context       : any
    max_instances : uint32
    -- outputs
    world         : any
end

struct ComponentDeleteWorldParams
    context       : any
    world         : any
end

struct ComponentCreateParams
    context       : any
    collection    : Collection
    instance      : Instance
    position      : @vmath.Point3
    rotation      : @vmath.Quat
    resource      : any
    world         : any
    component_index   : uint8
    -- outputs
    user_data     : any
end

struct ComponentDestroyParams
    context       : any
    collection    : Collection
    instance      : Instance
    world         : any
    user_data     : any
end

struct ComponentInitParams
    context       : any
    collection    : Collection
    instance      : Instance
    world         : any
    user_data     : any
end

struct ComponentFinalParams
    context       : any
    collection    : Collection
    instance      : Instance
    world         : any
    user_data     : any
end

struct ComponentAddToUpdateParams
    context       : any
    collection    : Collection
    instance      : Instance
    world         : any
    user_data     : any
end

struct ComponentsUpdateParams
    context       : any
    collection    : Collection
    world         : any
    dt            : float
end

struct ComponentsPostUpdateParams
    context       : any
    collection    : Collection
    world         : any
end

struct ComponentsRenderParams
    context       : any
    collection    : Collection
    world         : any
end

struct ComponentOnMessageParams
    context       : any
    instance      : Instance
    world         : any
    user_data     : any
    message       : interop.Handle
    message_id    : uint64
end

struct ComponentSetPropertiesParams
    -- UNIMPLEMENTED: Missing things.
    instance       : any
    user_data      : any
end

struct ComponentGetPropertyParams
    context        : any
    instance       : Instance
    world          : any
    property_id    : uint64
    user_data      : any
    value          : any
    element_ids    : @[4:uint64]
end

struct ComponentSetPropertyParams
    context        : any
    instance       : Instance
    world          : any
    property_id    : uint64
    user_data      : any
    -- property can be vmath.objects or any of the wrappers above
    value          : any
end

struct Collection
    proxy    : @interop.Proxy
end

struct Instance
    proxy    : @interop.Proxy
end

!symbol("SolGameObjectGetWorldMatrix")
extern get_world_matrix(inst:Instance, out:vmath.Matrix4)

!symbol("SolGameObjectGetScaleAlongZ")
extern get_scale_along_z(inst:Instance):bool

!symbol("SolGameObjectGetMessageDataInternal")
extern get_message_data_internal(msg:interop.Handle, empty_value:any):any

!symbol("SolGameObjectGetMessageDataInto")
extern get_message_data(msg:interop.Handle, into:any):bool

struct ComponentMessagePath
    instance       : handle
    component      : uint64
end

struct EmptyMsg

end

function get_message_data(msg:interop.Handle):any
    local x:EmptyMsg = nil
    return get_message_data_internal(msg, x);
end

!symbol("SolGameObjectGetMessageSenderComponentPath")
extern get_message_sender_component_path(inst:Instance, msg:interop.Handle, path:ComponentMessagePath):bool

!symbol("SolGameObjectPostMessage")
extern post_message(sender:Instance, sender_component_index:uint8, receiver:ComponentMessagePath, msg:any):bool
