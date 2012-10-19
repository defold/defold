package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CopyBuilder;

public class CopyBuilders {
    @BuilderParams(name = "GuiScript", inExts = ".gui_script", outExt = ".gui_scriptc")
    public static class GuiScriptBuilder extends CopyBuilder {}

    @BuilderParams(name = "RenderScript", inExts = ".render_script", outExt = ".render_scriptc")
    public static class RenderScriptBuilder extends CopyBuilder {}

    @BuilderParams(name = "Wav", inExts = ".wav", outExt = ".wavc")
    public static class WavBuilder extends CopyBuilder {}

    @BuilderParams(name = "Ogg", inExts = ".ogg", outExt = ".oggc")
    public static class OggBuilder extends CopyBuilder {}

    @BuilderParams(name = "Emitter", inExts = ".emitter", outExt = ".emitterc")
    public static class EmitterBuilder extends CopyBuilder {
    }

}
