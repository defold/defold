components {
  id: "script"
  component: "/main.script"
}
components {
  id: "gui"
  component: "/main.gui"
}
embedded_components {
  id: "loading_proxy"
  type: "collectionproxy"
  data: "collection: \"/loading/test_loading.collection\"\n"
}
embedded_components {
  id: "gui_proxy"
  type: "collectionproxy"
  data: "collection: \"/gui/test_gui.collection\"\n"
}
embedded_components {
  id: "attacker_target_proxy"
  type: "collectionproxy"
  data: "collection: \"/attacker_target/test_attacker_target.collection\"\n"
}
embedded_components {
  id: "identifiers_proxy"
  type: "collectionproxy"
  data: "collection: \"/identifiers/test_identifiers.collection\"\n"
}
embedded_components {
  id: "physics_proxy"
  type: "collectionproxy"
  data: "collection: \"/physics/test_physics.collection\"\n"
}
embedded_components {
  id: "factory_proxy"
  type: "collectionproxy"
  data: "collection: \"/factory/test_factory.collection\"\n"
}
embedded_components {
  id: "script_props_proxy"
  type: "collectionproxy"
  data: "collection: \"/script_props/test_script_props.collection\"\n"
}
embedded_components {
  id: "tile_proxy"
  type: "collectionproxy"
  data: "collection: \"/tile/test_tile.collection\"\n"
}
embedded_components {
  id: "final_post_proxy"
  type: "collectionproxy"
  data: "collection: \"/final_post/test_final_post.collection\"\n"
}

