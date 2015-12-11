module test_resource

require resource
require io
require ddf
require reflect

require test_ddf

!export("test_a_create")
function test_a_create(params : resource.ResourceCreateParams) : int
    local desc = ddf.from_buffer(reflect.type_of(test_ddf.AResource { }), params.buffer, params.data_offset, params.data_size) as test_ddf.AResource
    if nil(desc) then
        return resource.RESULT_FORMAT_ERROR
    end
    params.resource = desc
    return resource.RESULT_OK
end

!export("test_a_destroy")
function test_a_detroy(params : resource.ResourceDestroyParams) : int
    return resource.RESULT_OK
end

!export("test_b_create")
function test_b_create(params : resource.ResourceCreateParams) : int
    local desc = ddf.from_buffer(reflect.type_of(test_ddf.BResource { }), params.buffer, params.data_offset, params.data_size) as test_ddf.BResource
    if nil(desc) then
        return resource.RESULT_FORMAT_ERROR
    end
    params.resource = desc
    return resource.RESULT_OK
end

!export("test_b_destroy")
function test_b_detroy(params : resource.ResourceDestroyParams) : int
    return resource.RESULT_OK
end

!export("test_c_create")
function test_c_create(params : resource.ResourceCreateParams) : int
    local desc = ddf.from_buffer(reflect.type_of(test_ddf.CResource { }), params.buffer, params.data_offset, params.data_size) as test_ddf.CResource
    if nil(desc) then
        return resource.RESULT_FORMAT_ERROR
    end
    params.resource = desc
    return resource.RESULT_OK
end

!export("test_c_destroy")
function test_c_detroy(params : resource.ResourceDestroyParams) : int
    return resource.RESULT_OK
end

