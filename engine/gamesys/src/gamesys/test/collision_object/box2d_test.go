components {
  id: "script"
  component: "/collision_object/box2d_test.script"
}
embedded_components {
  id: "factory"
  type: "factory"
  data: "prototype: \"/collision_object/joint_test_b.go\"\n"
  "load_dynamically: false\n"
}
