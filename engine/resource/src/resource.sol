module resource

require io
require reflect
require interop

----------------------------------
-- Public interface
----------------------------------

let RESULT_OK:int = 0
let RESULT_PENDING:int = -17
let RESULT_FORMAT_ERROR:int = -13

struct Factory
    local proxy : [@interop.Proxy]
end

-- This is the params offered to SOL implementations
struct ResourcePreloadParams
    -- inputs
    factory       : Factory
    filename      : String
    buffer        : [uint8]
    data_offset   : uint32
    data_size     : uint32    
    -- outputs
    preload_data  : any
    hint_info     : interop.Handle
end

struct ResourceCreateParams
    -- inputs
    factory       : Factory
    filename      : String
    buffer        : [uint8]
    data_offset   : uint32
    data_size     : uint32    
    preload_data  : any
    -- outputs
    resource      : any
end

struct ResourceDestroyParams
    -- inputs
    factory       : Factory
    resource      : any
end

struct ResourceRecreateParams
    -- inputs
    factory       : Factory
    filename      : String
    buffer        : [uint8]
    data_offset   : uint32
    data_size     : uint32    
    -- outputs
    resource      : any
end

struct ResourceDescriptor
    resource    : any
end

struct InternalResource
    -- unusable from sol
end

function get(factory:Factory, path:String, output:ResourceDescriptor):int
        local tmp:InternalResource = nil
        output.resource = tmp
        return get_internal(factory, path, output)
end

!symbol("SolResourceRelease")
extern release(factory:Factory, resource:handle)

!symbol("SolPreloadHint")
extern preload_hint(hint_info:interop.Handle, path:String)

!symbol("SolResourceGet")
extern get_internal(factory:Factory, path:String, output:ResourceDescriptor):int
