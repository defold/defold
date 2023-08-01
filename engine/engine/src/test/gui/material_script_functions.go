components {
  id: "gui"
  component: "/gui/material_script_functions.gui"
}
components {
  id: "script"
  component: "/gui/material_script_functions.script"

  properties {
    id: "my_material"
    value: "/gui/material_script_functions_c.material"
    type: PROPERTY_TYPE_HASH
  }
}
