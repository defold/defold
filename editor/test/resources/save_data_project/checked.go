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
  properties {
    id: "number"
    value: "1.0"
    type: PROPERTY_TYPE_NUMBER
  }
  properties {
    id: "texture"
    value: "/referenced/images/red.png"
    type: PROPERTY_TYPE_HASH
  }
}
embedded_components {
  id: "embedded_sprite"
  type: "sprite"
  data: "tile_set: \"/checked.atlas\"\n"
  "default_animation: \"diamond\"\n"
  "material: \"/checked.material\"\n"
  "blend_mode: BLEND_MODE_ALPHA\n"
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
