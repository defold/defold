package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;

public class ScriptBuilders {
    @BuilderParams(name = "Lua", inExts = ".lua", outExt = ".luac")
    public static class LuaScriptBuilder extends LuaBuilder {}

    @BuilderParams(name = "Script", inExts = ".script", outExt = ".scriptc")
    public static class ScriptBuilder extends LuaBuilder {}

    @BuilderParams(name = "GuiScript", inExts = ".gui_script", outExt = ".gui_scriptc")
    public static class GuiScriptBuilder extends LuaBuilder {}

    @BuilderParams(name = "RenderScript", inExts = ".render_script", outExt = ".render_scriptc")
    public static class RenderScriptBuilder extends LuaBuilder {}

}
