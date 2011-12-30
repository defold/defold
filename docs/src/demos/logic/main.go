components {
  id: "script"
  component: "/logic/main.script"
}
components {
  id: "title"
  component: "/logic/title.gui"
}
embedded_components {
  id: "demo01"
  type: "collectionproxy"
  data: "collection: \"/demo01/demo01.collection\"\n"
}
embedded_components {
  id: "main_menu"
  type: "collectionproxy"
  data: "collection: \"/main_menu/main.collection\"\n"
}
embedded_components {
  id: "hud"
  type: "collectionproxy"
  data: "collection: \"/hud/main.collection\"\n"
}
