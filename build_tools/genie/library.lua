
local M = {}


function M.setup_project(proj_dir, log_domain)
    local BUILD_DIR = path.join(proj_dir, "build/" .. _ACTION)
    local GEN_DIR = path.join(proj_dir, "build/gen")

    solution "Defold"
        location( BUILD_DIR ) -- the solution files

        targetdir( BUILD_DIR ) -- the binary output files

        debugdir (proj_dir) -- the working directory when debugging the binary files

        defines {
            string.format("DLIB_LOG_DOMAIN=\\\"%s\\\"", log_domain),
            "GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED",
            "GL_SILENCE_DEPRECATION",
        }

        includedirs {
            path.join(proj_dir, "src/include"),
            path.join(proj_dir, "src"),
        }

    m = {}
    m.BUILD_DIR = BUILD_DIR
    m.GEN_DIR = GEN_DIR
    return m
end

function M.run_command(command)
    local handle = io.popen(command)
    local result = handle:read("*a")
    handle:close()
    print(result)
end

return M
