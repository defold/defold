components {
  id: "sprite"
  component: "/logic/session/ball.sprite"
}
components {
  id: "script"
  component: "/logic/session/ball.script"
}
components {
  id: "wav"
  component: "/sounds/tink.wav"
}
components {
  id: "spine"
  component: "/spine/reload.spinemodel"
}
embedded_components {
  id: "co"
  type: "collisionobject"
  data: "collision_shape: \"/logic/session/ball.convexshape\"\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\nmass: 0.0\nfriction: 0.0\nrestitution: 1.0\ngroup: \"4\"\nmask: \"1\"\nmask: \"2\"\nmask: \"8\"\n"
}
embedded_components {
  id: "factory"
  type: "factory"
  data: "prototype: \"/logic/session/pow.go\"\n"
}
