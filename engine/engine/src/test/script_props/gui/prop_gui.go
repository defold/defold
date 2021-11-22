components {
  id: "gui"
  component: "/script_props/gui/prop_gui.gui"
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
}
components {
  id: "prop_guiscript"
  component: "/script_props/gui/prop_gui.script"
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
    id: "big_font"
    value: "/script_props/gui/system_font_big.font"
    type: PROPERTY_TYPE_HASH
  }
}
