module render_private

require render

!export("sol_render_alloc_material")
function alloc_material():any
    return render.Material { }
end

!export("sol_render_alloc_dispatch_params")
function sol_render_alloc_dispatch_params():any
    return render.RenderListDispatchParams { }
end

