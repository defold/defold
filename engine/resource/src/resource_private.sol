module resource_private

require resource

!export("sol_resource_alloc_preload_params")
function alloc_preload_params() : resource.ResourcePreloadParams
        return resource.ResourcePreloadParams { }
end

!export("sol_resource_alloc_create_params")
function alloc_create_params() : resource.ResourceCreateParams
        return resource.ResourceCreateParams { }
end

!export("sol_resource_alloc_destroy_params")
function alloc_destroy_params() : resource.ResourceDestroyParams
        return resource.ResourceDestroyParams { }
end

!export("sol_resource_alloc_recreate_params")
function alloc_recreate_params() : resource.ResourceRecreateParams
        return resource.ResourceRecreateParams { }
end
