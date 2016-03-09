package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CopyBuilder;

public class CopyBuilders {

    @BuilderParams(name = "Wav", inExts = ".wav", outExt = ".wavc")
    public static class WavBuilder extends CopyBuilder {}

    @BuilderParams(name = "Ogg", inExts = ".ogg", outExt = ".oggc")
    public static class OggBuilder extends CopyBuilder {}

    @BuilderParams(name = "Emitter", inExts = ".emitter", outExt = ".emitterc")
    public static class EmitterBuilder extends CopyBuilder {
    }

    @BuilderParams(name = "Gamepads", inExts = ".gamepads", outExt = ".gamepadsc")
    public static class GamepadsBuilder extends CopyBuilder {}

}
