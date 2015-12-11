module gamesys_private
require gamesys

!export("gamesys_alloc_sprite_context")
function alloc_sprite_context():any
    return gamesys.SpriteContext {
    }
end

