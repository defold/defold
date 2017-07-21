components {
  id: "gui"
  component: "/logic/session/hud.gui"
}
components {
  id: "wav"
  component: "/sounds/tink.wav"
}
embedded_components {
  id: "collectionproxy"
  type: "collectionproxy"
  data: "collection: \"/\"\nexclude: false\n"
}
