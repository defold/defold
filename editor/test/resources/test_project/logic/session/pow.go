components {
  id: "sprite"
  component: "/logic/session/pow.sprite"
}
components {
  id: "script"
  component: "/logic/session/pow.script"
}
embedded_components {
  id: "co"
  type: "collisionobject"
  data: "collision_shape: \"/logic/session/pow.convexshape\"\ntype: COLLISION_OBJECT_TYPE_TRIGGER\nmass: 0.0\nfriction: 0.0\nrestitution: 1.0\ngroup: \"16\"\nmask: \"2\"\nlinear_damping: 0.0\nangular_damping: 0.0\nlocked_rotation: false\n"
}
