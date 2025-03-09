
local M = {}

M.DYNAMO_HOME = os.getenv("DYNAMO_HOME")
M.TARGET_PLATFORM = nil -- use --target=<platform>

-- how to create a custom toolset
-- https://stackoverflow.com/questions/35116445/create-own-toolset-in-premake5
-- https://github.com/premake/premake-core/blob/master/src/tools/clang.lua

newoption {
    trigger     = "target",
    description = "Platform target",
    allowed     = {
        { "x86_64-macos", ""},
        { "arm64-macos", ""},
        { "x86_64-linux", ""},
        { "arm64-linux", ""},
        { "x86_64-win32", ""},
    }
}

function M.get_host_platform()
    -- https://github.com/bkaradzic/GENie/blob/master/docs/scripting-reference.md#osget
    if os.get() == "windows" then
        return os.is64bit() and "x86_64-win32" or "win32"
    end
    if os.get() == "macosx" then
        return os.is64bit() and "x86_64-macos" or "macos"
    end
    if os.get() == "linux" then
        return os.is64bit() and "x86_64-linux" or "linux"
    end
end

function M.setup()

    if _OPTIONS['target'] == nil then
        _OPTIONS['target'] = M.get_host_platform()
    end

    print("target platform:", _OPTIONS['target'])
    M.TARGET_PLATFORM=_OPTIONS['target']

    solution "Defold"
        configurations {
            "Debug",
            "Release",
        }

        -- platforms {
        --     "x32",
        --     "x64"
        -- }

        language "C++"
        --toolset ("clang") -- https://premake.github.io/docs/toolset/

        includedirs {
            path.join(M.DYNAMO_HOME, "include"),
            path.join(M.DYNAMO_HOME, "sdk/include"),
            path.join(M.DYNAMO_HOME, "ext/include"),
        }

        configuration {"x86_64-macos or arm64-macos"}
            xcodeprojectopts {
                MACOSX_DEPLOYMENT_TARGET = "10.13",
            }

        libdirs {
            path.join(M.DYNAMO_HOME, "lib/" .. _OPTIONS['target']),
            path.join(M.DYNAMO_HOME, "ext/lib/" .. _OPTIONS['target']),
        }

        configuration {}

end

function M.get_libs_installdir()
    if M.TARGET_PLATFORM == nil then
        return DYNAMO_HOME .. "/lib"
    end
    return DYNAMO_HOME .. "/lib/" .. M.TARGET_PLATFORM
end

return M

