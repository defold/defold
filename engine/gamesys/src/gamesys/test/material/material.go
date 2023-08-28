components {
  id: "script"
  component: "/material/material.script"
}

embedded_components {
  id: "constant_array"
  type: "sprite"
  data: "tile_set: \"/tile/flipbook.tilesource\"\n"
  "default_animation: \"anim\"\n"
  "material: \"/material/material_constant_array.material\"\n"
  "blend_mode: BLEND_MODE_ALPHA\n"
}

embedded_components {
  id: "constant_single"
  type: "sprite"
  data: "tile_set: \"/tile/flipbook.tilesource\"\n"
  "default_animation: \"anim\"\n"
  "material: \"/material/material_constant_single.material\"\n"
  "blend_mode: BLEND_MODE_ALPHA\n"
}
