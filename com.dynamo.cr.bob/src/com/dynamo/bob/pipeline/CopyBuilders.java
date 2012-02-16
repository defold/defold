package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CopyBuilder;

public class CopyBuilders {
    @BuilderParams(name = "Project", inExts = ".project", outExt = ".projectc")
    public static class ProjectBuilder extends CopyBuilder {}

    @BuilderParams(name = "Script", inExts = ".script", outExt = ".scriptc")
    public static class ScriptBuilder extends CopyBuilder {}

    @BuilderParams(name = "GuiScript", inExts = ".gui_script", outExt = ".gui_scriptc")
    public static class GuiScriptBuilder extends CopyBuilder {}

    @BuilderParams(name = "RenderScript", inExts = ".render_script", outExt = ".render_scriptc")
    public static class RenderScriptBuilder extends CopyBuilder {}

    @BuilderParams(name = "Wav", inExts = ".wav", outExt = ".wavc")
    public static class WavBuilder extends CopyBuilder {}

    // TODO: Vertex and Fragment-program builder should contain validation
    // through cgc (CG compiler)
    @BuilderParams(name = "VertexProgram", inExts = ".vp", outExt = ".vpc")
    public static class VertexProgramBuilder extends CopyBuilder {}

    @BuilderParams(name = "FragmentProgram", inExts = ".fp", outExt = ".fpc")
    public static class FragmentProgramBuilder extends CopyBuilder {}
}
