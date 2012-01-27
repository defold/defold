components {
  id: "script"
  component: "/logic/hero/hero.script"
  position {
    x: 0.0
    y: 0.0
    z: 0.0
  }
  rotation {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
}
embedded_components {
  id: "collisionobject"
  type: "collisionobject"
  data: "collision_shape: \"/logic/hero/hero.convexshape\"\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\nmass: 0.0\nfriction: 0.1\nrestitution: 0.5\ngroup: \"hero\"\nmask: \"platform\"\nmask: \"bridge\"\nmask: \"ladder\"\n"
  position {
    x: 0.0
    y: 0.0
    z: 0.0
  }
  rotation {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
}
embedded_components {
  id: "sprite2"
  type: "sprite2"
  data: "tile_set: \"/logic/hero/hero.tileset\"\ndefault_animation: \"idle_right\"\n"
  position {
    x: 0.0
    y: 0.0
    z: 0.0
  }
  rotation {
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }
}
