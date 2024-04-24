components {
  id: "referenced_sprite"
  component: "/checked.sprite"
  position {
    x: 0.1
    y: 0.2
    z: 0.3
  }
  rotation {
    x: 8.7722944E-4
    y: 0.0017476063
    z: 0.0026164628
    w: 0.99999464
  }
  scale {
    x: 1.1
    y: 1.2
    z: 1.3
  }
}
components {
  id: "referenced_script"
  component: "/checked.script"
}
embedded_components {
  id: "embedded_sprite"
  type: "sprite"
  data: "default_animation: \"diamond\"\n"
  "material: \"/checked.material\"\n"
  "textures {\n"
  "  sampler: \"texture0\"\n"
  "  texture: \"/checked.atlas\"\n"
  "}\n"
  ""
  position {
    x: 0.1
    y: 0.2
    z: 0.3
  }
  rotation {
    x: 8.7722944E-4
    y: 0.0017476063
    z: 0.0026164628
    w: 0.99999464
  }
  scale {
    x: 1.1
    y: 1.2
    z: 1.3
  }
}
