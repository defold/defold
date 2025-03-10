
if _ACTION == nil then
    return nil
end

DYNAMO_HOME = os.getenv("DYNAMO_HOME")
DEFOLD_HOME = DYNAMO_HOME .. "/../.."
DEFOLD_SCRIPT_PATH = DEFOLD_HOME .. "/build_tools/genie"

local paths = {
    string.format('%s/?.lua', DEFOLD_SCRIPT_PATH),
    package.path,
}
package.path = table.concat(paths, ';')

local dm_solution = require("solution")
local dm_library = require("library")

dm_solution.setup()

-------------------------------------------------------------------------------------------------------------------
-- Local project settings

PROJ_DIR = path.getabsolute("..")
proj = dm_library.setup_project(PROJ_DIR, "HID")

-------------------------------------------------------------------------------------------------------------------
-- Local projects
solution "Defold"

    group "HID"

        project "hid_null"
            kind "StaticLib"

            files {
                path.join(PROJ_DIR, "src/hid_null.cpp"),
                path.join(PROJ_DIR, "src/hid.cpp"),
                path.join(PROJ_DIR, "src/**/*.h"),
            }

        project "hid"
            kind "StaticLib"

            files {
                path.join(PROJ_DIR, "src/native/*.cpp"),
                path.join(PROJ_DIR, "src/hid.cpp"),
                path.join(PROJ_DIR, "src/**/*.h"),
            }

    -------------------------------------------------------------------------------------------------------------------
    -- Tests

    group "HID - Tests"

        project "test_hid"
            kind "ConsoleApp"
            wholearchive { "hid_null" } --tests use unstripped libraries

            configuration {}
                links { "hid_null", "dlib", "profile_null", "platform_null"}

            files {
                path.join(PROJ_DIR, "src/test/test_hid.cpp"),
                path.join(PROJ_DIR, "src/test/*.h"),
            }

        project "test_app_hid"
            kind "ConsoleApp"

            links { "hid", "dlib", "profile_null", "platform", "glfw3", "graphics"}
            wholearchive { "hid" } --tests use unstripped libraries

            configuration {"x86_64-macos or arm64-macos"}
                links { "Cocoa.framework", "QuartzCore.framework", "IOKit.framework", "OpenGL.framework", "CoreVideo.framework" }

            -- generate the file upfront
            local exported_symbols = {"GraphicsAdapterOpenGL"}
            dm_library.run_command(string.format("python %s %s %s", path.join(DEFOLD_SCRIPT_PATH, "gen_exported_symbols.py"), path.join(proj.GEN_DIR, "exported_symbols.cpp"), table.concat(exported_symbols, " ")))

            -- prebuildcommands {
            --         string.format("python %s %s %s", path.join(DEFOLD_SCRIPT_PATH, "gen_exported_symbols.py"), path.join(proj.GEN_DIR, "exported_symbols.cpp"), table.concat(exported_symbols, " ")),
            --     }

            files {
                path.join(proj.GEN_DIR, "exported_symbols.cpp"),
                path.join(PROJ_DIR, "src/test/test_app_hid.cpp"),
                path.join(PROJ_DIR, "src/test/*.h"),
            }

-------------------------------------------------------------------------------------------------------------------
-- Install

configuration {}
    postbuildcommands {
        string.format("echo INSTALLING from %s to %s", proj.BUILD_DIR, dm_solution.get_libs_installdir()),
        string.format("mkdir -p %s", dm_solution.get_libs_installdir()),
        string.format("cp -v %s/*.a %s", proj.BUILD_DIR, dm_solution.get_libs_installdir()),
    }
