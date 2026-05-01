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

import java.io.IOException;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

public class CopyBuilders {

    @BuilderParams(name = "Wav", inExts = ".wav", outExt = ".wavc", paramsForSignature = {"sound-stream-enabled"})
    public static class WavBuilder extends CopyBuilder {
        @Override
        public void build(Task task) throws IOException, CompileExceptionError {
            super.build(task);

            boolean soundStreaming = this.project.option("sound-stream-enabled", "false").equals("true"); // if no value set use old hardcoded path (backward compatability)
            boolean compressSounds = soundStreaming ? false : true; // We want to be able to read directly from the files as-is (without compression)
            for(IResource res : task.getOutputs()) {
                if (!compressSounds) {
                    project.addOutputFlags(res.getAbsPath(), Project.OutputFlags.UNCOMPRESSED);
                }
            }
        }
    }

    @BuilderParams(name = "Gamepads", inExts = ".gamepads", outExt = ".gamepadsc")
    public static class GamepadsBuilder extends CopyBuilder {}

    @BuilderParams(name = "Glsl", inExts = ".glsl", outExt = ".glslc")
    public static class GlslBuilder extends CopyBuilder {}

    @BuilderParams(name = "TTF", inExts = ".ttf", outExt = ".ttf")
    public static class TTFBuilder extends CopyBuilder {}
}
