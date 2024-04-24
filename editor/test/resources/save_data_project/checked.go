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
  properties {
    id: "boolean"
    value: "true"
    type: PROPERTY_TYPE_BOOLEAN
  }
  properties {
    id: "number"
    value: "1.0"
    type: PROPERTY_TYPE_NUMBER
  }
  properties {
    id: "vector3"
    value: "0.1, 0.2, 0.3"
    type: PROPERTY_TYPE_VECTOR3
  }
  properties {
    id: "vector4"
    value: "0.1, 0.2, 0.3, 0.4"
    type: PROPERTY_TYPE_VECTOR4
  }
  properties {
    id: "quat"
    value: "8.772E-4, 0.0017476, 0.0026165, 0.9999947"
    type: PROPERTY_TYPE_QUAT
  }
  properties {
    id: "url"
    value: "#referenced_script"
    type: PROPERTY_TYPE_URL
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
