components { id: "script" component: "/main.script" }
components { id: "label" component: "/label.label" }
components { id: "map" component: "/tiles.tilemap" }
components { id: "gui" component: "/overlay.gui" }
components { id: "effect" component: "/effect.particlefx" }
components { id: "sound" component: "/tone.sound" }
embedded_components { id: "factory" type: "factory" data: "prototype: \"/child.go\"" }
embedded_components { id: "geometry_factory" type: "factory" data: "prototype: \"/geometry_child.go\"" }
embedded_components { id: "collectionfactory" type: "collectionfactory" data: "prototype: \"/child.collection\"" }
embedded_components { id: "proxy" type: "collectionproxy" data: "collection: \"/proxy.collection\"\nexclude: false" }
