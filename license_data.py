# Only these file types receive a Defold License header from apply_license.py.
# Format: ".extension": "line-comment prefix " (including the trailing space).
# For example, ".h": "// " and ".clj": ";; ".
ext_to_license = {
    ".h":             "// ",
    ".hpp":           "// ",
    ".c":             "// ",
    ".cpp":           "// ",
    ".inl":           "// ",
    ".m":             "// ",
    ".mm":            "// ",
    ".java":          "// ",
    ".cs":            "// ",
    ".proto":         "// ",
    ".clj":           ";; ",
}

# Individual files skipped by apply_license.py because they retain upstream notices.
excluded_files = [
    # Google-derived source retains its upstream licence and modification notice.
    "engine/ddf/src/ddf/ddf_struct.proto",
    # Third-party source files retain their upstream notices.
    "engine/font/src/test/stb_truetype.h",
    "engine/hid/src/external/glfw/mappings.h",
    "engine/hid/src/external/sdl/joystick/usb_ids.h",
]

# Directories skipped by apply_license.py to avoid changing existing vendor or fixture files.
# New first-party files of the types above still need a Defold License header here.
excluded_paths = [
    "./.git",
    "./external",
    "./editor/src/clj/potemkin",
    "./editor/test/resources/save_data_project",
    "./engine/dlib/src/mbedtls/include",
    "./engine/dlib/src/mbedtls/library",
    "./engine/dlib/src/mbedtls/tf-psa-crypto",
    "./engine/dlib/src/stb",
    "./engine/dlib/src/jc",
    "./engine/dlib/src/zip",
    "./engine/graphics/src/opengl/win32",
    "./engine/lua/src/lua",
    "./engine/modelc/src/cgltf",
    "./engine/profiler/src/remotery/lib",
    "./engine/sound/src/stb_vorbis",
    "./engine/script/src/bitop",
    "./engine/script/src/luasocket",
]
