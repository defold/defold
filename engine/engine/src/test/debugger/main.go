components {
  id: "body"
  component: "/physics/kinematic.collisionobject"
}
components {
  id: "other_body"
  component: "/physics/static.collisionobject"
}
components {
  id: "script"
  component: "/debugger/main.script"
}
components {
  id: "gui"
  component: "/debugger/main.gui"
}
embedded_components {
  id: "factory"
  type: "factory"
  data: "prototype: \"/debugger/callback.go\""
}
