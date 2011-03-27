components {
  id: "script"
  component: "logic/main.script"
}
components {
  id: "gui"
  component: "logic/main.gui"
}
embedded_components {
  id: "session_proxy"
  type: "collectionproxy"
  data: "collection: \"logic/session/session.collection\"\n"
}
