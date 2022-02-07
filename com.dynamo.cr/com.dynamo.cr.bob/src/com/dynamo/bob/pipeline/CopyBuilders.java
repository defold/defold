// Copyright 2020 The Defold Foundation
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
