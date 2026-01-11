// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.lua.proto.Lua.LuaModule;

public class ScriptBuilders {

    @ProtoParams(srcClass = LuaModule.class, messageClass = LuaModule.class)
    @BuilderParams(name = "Lua", inExts = ".lua", outExt = ".luac", paramsForSignature = {"use-uncompressed-lua-source", "use-lua-bytecode-delta", "platform", "architectures", "variant", "prometheus-disabled"})
    public static class LuaScriptBuilder extends LuaBuilder {}

    @ProtoParams(srcClass = LuaModule.class, messageClass = LuaModule.class)
    @BuilderParams(name = "Script", inExts = ".script", outExt = ".scriptc", paramsForSignature = {"use-uncompressed-lua-source", "use-lua-bytecode-delta", "platform", "architectures", "variant", "prometheus-disabled"})
    public static class ScriptBuilder extends LuaBuilder {}

    @ProtoParams(srcClass = LuaModule.class, messageClass = LuaModule.class)
    @BuilderParams(name = "GuiScript", inExts = ".gui_script", outExt = ".gui_scriptc", paramsForSignature = {"use-uncompressed-lua-source", "use-lua-bytecode-delta", "platform", "architectures", "variant", "prometheus-disabled"})
    public static class GuiScriptBuilder extends LuaBuilder {}

    @ProtoParams(srcClass = LuaModule.class, messageClass = LuaModule.class)
    @BuilderParams(name = "RenderScript", inExts = ".render_script", outExt = ".render_scriptc", paramsForSignature = {"use-uncompressed-lua-source", "use-lua-bytecode-delta", "platform", "architectures", "variant", "prometheus-disabled"})
    public static class RenderScriptBuilder extends LuaBuilder {}

}
