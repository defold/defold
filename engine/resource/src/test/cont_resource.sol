module cont_resource

require foo_resource
require resource
require io
require ddf
require reflect
require test_resource_ddf

struct Cont
    foos     : [foo_resource.Foo]
end

!export("test_cont_create")
function test_cont_create(params : resource.ResourceCreateParams) : int

    local desc = ddf.from_buffer(reflect.type_of(test_resource_ddf.ResourceContainerDesc { }), params.buffer, params.data_offset, params.data_size) as test_resource_ddf.ResourceContainerDesc
    if nil(desc) then
        return resource.RESULT_FORMAT_ERROR
    end

    local out = Cont {
        foos = [#desc.resources:foo_resource.Foo]
    }

    for i,path in desc.resources do
        local get_desc = resource.ResourceDescriptor { }
        local ret = resource.get(params.factory, desc.resources[i], get_desc)
        if ret ~= resource.RESULT_OK then
           return ret
        end
        out.foos[i] = get_desc.resource as foo_resource.Foo
    end

    params.resource = out
    return resource.RESULT_OK
end

!export("test_cont_destroy")
function test_cont_detroy(params : resource.ResourceDestroyParams) : int
    local cont = params.resource as Cont
    for i,res in cont.foos do
        resource.release(params.factory, res)
    end
    return resource.RESULT_OK
end
