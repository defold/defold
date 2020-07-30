components {
  id: "sprite"
  component: "/logic/session/block.sprite"
}
components {
  id: "script"
  component: "/logic/session/block.script"
}
embedded_components {
  id: "co"
  type: "collisionobject"
  data: "collision_shape: \"/logic/session/block.convexshape\"\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\nmass: 0.0\nfriction: 0.0\nrestitution: 1.0\ngroup: \"8\"\nmask: \"4\"\nlinear_damping: 0.0\nangular_damping: 0.0\nlocked_rotation: false\n"
}
