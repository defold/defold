
local M = {}


M.BUILD_DIR = "build/default"
M.GEN_DIR = "build/gen"


function M.setup_project(proj_dir, log_domain)

    solution "Defold"
        location( path.join(proj_dir, "build/" .. _ACTION) ) -- the solution files

        targetdir( path.join(proj_dir, M.BUILD_DIR) ) -- the binary output files

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
    m.build_dir = proj_dir .. "/" .. M.BUILD_DIR
    return m
end


return M
