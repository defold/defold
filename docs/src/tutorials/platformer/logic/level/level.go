components {
  id: "tilegrid"
  component: "/logic/level/level.tilegrid"
}
embedded_components {
  id: "collisionobject"
  type: "collisionobject"
  data: "collision_shape: \"/logic/level/level.tilegrid\"\ntype: COLLISION_OBJECT_TYPE_STATIC\nmass: 0.0\nfriction: 0.1\nrestitution: 0.5\ngroup: \"default\"\nmask: \"hero\"\n"
}
