components {
  id: "script"
  component: "/logic/hero/hero.script"
}
embedded_components {
  id: "sprite"
  type: "sprite"
  data: "texture: \"/graphics/hero_sheet.png\"\nwidth: 64.0\nheight: 64.0\ntile_width: 64\ntile_height: 64\ntiles_per_row: 12\ntile_count: 48\n"
}
embedded_components {
  id: "collisionobject"
  type: "collisionobject"
  data: "collision_shape: \"/logic/hero/hero.convexshape\"\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\nmass: 0.0\nfriction: 0.1\nrestitution: 0.5\ngroup: \"hero\"\nmask: \"platform\"\nmask: \"bridge\"\nmask: \"ladder\"\n"
}
