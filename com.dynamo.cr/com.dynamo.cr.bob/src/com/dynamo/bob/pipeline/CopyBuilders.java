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
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

public class CopyBuilders {

    @BuilderParams(name = "Wav", inExts = ".wav", outExt = ".wavc")
    public static class WavBuilder extends CopyBuilder {
        @Override
        public Task create(IResource input) throws IOException, CompileExceptionError {
            boolean soundStreaming = project.option("sound-stream-enabled", "false").equals("true");
            // Streamed sounds must be readable directly from the archive.
            return soundStreaming ? defaultTask(input, Task.OutputFlags.UNCOMPRESSED) : super.create(input);
        }
    }


    @BuilderParams(name = "Glsl", inExts = ".glsl", outExt = ".glslc")
    public static class GlslBuilder extends CopyBuilder {}

    @BuilderParams(name = "TTF", inExts = ".ttf", outExt = ".ttf")
    public static class TTFBuilder extends CopyBuilder {}

    @BuilderParams(name = "OTF", inExts = ".otf", outExt = ".otf")
    public static class OTFBuilder extends CopyBuilder {}
}
