SPECIAL_FILENAMES = {"display_profiles"}

SKIPPED_DIR_NAMES = {
    ".git",
    ".idea",
    ".internal",
    ".vscode",
    "build",
    "node_modules",
}

PROTO_TEXT_EXTENSIONS = (
    "atlas",
    "camera",
    "collection",
    "collectionfactory",
    "collectionproxy",
    "factory",
    "go",
    "gui",
    "label",
    "material",
    "model",
    "particlefx",
    "render",
    "sound",
    "sprite",
    "tilemap",
    "tilesource",
)

PROFILE_RELEVANT_EXTENSIONS = (
    "atlas",
    "collection",
    "collectionproxy",
    "font",
    "fp",
    "go",
    "gui",
    "gui_script",
    "lua",
    "material",
    "particlefx",
    "png",
    "render",
    "render_script",
    "script",
    "tilesource",
    "vp",
)

PROFILE_RELEVANT_TOPOLOGY_KEYS = (
    "collection_instance_count",
    "collection_subcollection_count",
    "go_component_count",
    "gui_max_node_depth",
    "gui_node_count",
    "gui_node_depth",
    "gui_parented_node_count",
    "gui_template_node_count",
    "lua_require_count",
    "lua_require_resolved_count",
    "particlefx_emitter_count",
    "render_material_slot_count",
)
