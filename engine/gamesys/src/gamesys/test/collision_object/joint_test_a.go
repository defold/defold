components {
  id: "collisionobject"
  component: "/collision_object/joint_test_sphere_kinematic.collisionobject"
}
components {
  id: "script"
  component: "/collision_object/joint_test.script"
}
embedded_components {
  id: "factory"
  type: "factory"
  data: "prototype: \"/collision_object/joint_test_b.go\"\n"
  "load_dynamically: false\n"
  ""
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

