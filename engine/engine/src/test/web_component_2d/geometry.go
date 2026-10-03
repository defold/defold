components { id: "script" component: "/geometry.script" }
components { id: "mesh_world" component: "/mesh_world.mesh" position { x: 30 y: 260 } }
components { id: "mesh_local" component: "/mesh_local.mesh" position { x: 110 y: 260 } }
components { id: "static" component: "/model_local.model" position { x: 190 y: 260 } }
components { id: "cpu" component: "/model_world.model" position { x: 270 y: 260 } }
components { id: "gpu" component: "/model_skinned.model" position { x: 350 y: 260 } }
components { id: "instanced_a" component: "/model_instanced.model" position { x: 430 y: 260 } }
components { id: "instanced_b" component: "/model_instanced.model" position { x: 510 y: 260 } }
