components {
  id: "script"
  component: "/script/props.script"
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
    value: "2.0"
    type: PROPERTY_TYPE_NUMBER
  }
  properties {
    id: "hash"
    value: "hash2"
    type: PROPERTY_TYPE_HASH
  }
  properties {
    id: "url"
    value: "/url"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "vec3"
    value: "1.0, 2.0, 3.0"
    type: PROPERTY_TYPE_VECTOR3
  }
  properties {
    id: "quat"
    value: "1.0, 0.0, 0.0, 0.0"
    type: PROPERTY_TYPE_QUAT
  }
  properties {
    id: "vec4"
    value: "1.0, 2.0, 3.0, 4.0"
    type: PROPERTY_TYPE_VECTOR4
  }
  properties {
    id: "bool"
    value: "true"
    type: PROPERTY_TYPE_BOOLEAN
  }
  properties {
    id: "atlas"
    value: "/script/resources/from_props_game_object.atlas"
    type: PROPERTY_TYPE_HASH
  }
  properties {
    id: "material"
    value: "/script/resources/from_props_game_object.material"
    type: PROPERTY_TYPE_HASH
  }
  properties {
    id: "texture"
    value: "/script/resources/from_props_game_object.png"
    type: PROPERTY_TYPE_HASH
  }
}
