components {
  id: "script"
  component: "/logic/main.script"
}
components {
  id: "gui"
  component: "/logic/main.gui"
}
components {
  id: "camera"
  component: "/logic/test.camera"
}
embedded_components {
  id: "session_proxy"
  type: "collectionproxy"
  data: "collection: \"/logic/session/session.collection\"\nexclude: false\n"
}
