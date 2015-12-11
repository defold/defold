module graphics_private

require graphics

!export("sol_graphics_alloc_texture")
function sol_graphics_alloc_texture():any
        return graphics.Texture { }
end

!export("sol_graphics_alloc_vertex_declaration")
function sol_graphics_alloc_vertex_declaration():any
        return graphics.VertexDeclaration { }
end

!export("sol_graphics_alloc_vertex_buffer")
function sol_graphics_alloc_vertex_buffer():any
        return graphics.VertexBuffer { }
end
