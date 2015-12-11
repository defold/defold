module foo_resource

require resource
require io

struct Foo
    filename : String
    data     : [uint8]
end

!export("test_foo_create")
function test_foo_create(params : resource.ResourceCreateParams) : int
    params.resource = Foo {
        filename = params.filename,
        data = params.buffer
    }
    return resource.RESULT_OK
end

!export("test_foo_destroy")
function test_foo_destroy(params : resource.ResourceDestroyParams) : int
    local r:Foo = params.resource as Foo
    return resource.RESULT_OK
end

!export("test_foo_recreate")
function test_foo_recreate(params : resource.ResourceRecreateParams) : int
    params.resource = Foo {
        filename = params.filename,
        data = params.buffer
    }
    return resource.RESULT_OK
end
