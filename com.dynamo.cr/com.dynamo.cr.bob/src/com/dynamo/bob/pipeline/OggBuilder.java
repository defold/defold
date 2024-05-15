// Copyright 2020-2024 The Defold Foundation
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
import java.util.List;
import java.io.File;
import java.io.BufferedOutputStream;
import java.io.FileOutputStream;

import com.dynamo.bob.Bob;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

@BuilderParams(name = "Ogg", inExts = ".ogg", outExt = ".oggc")
public class OggBuilder extends CopyBuilder{

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Platform curr_platform = Platform.getHostPlatform();
        List<String> deps = List.of("libogg", "liboggz");
        Bob.unpackSharedLibraries(curr_platform, deps);
        File tmpOggFile = null;
        try {
            tmpOggFile = File.createTempFile("ogg_tmp", null, Bob.getRootFolder());
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(tmpOggFile));
            try {
                os.write(input.getContent());
            } finally {
                os.close();
            }
        } catch (IOException exc) {
            throw new CompileExceptionError(input, 0, 
                String.format("Cannot copy ogg file to further process", new String(exc.getMessage())));
        }
        Result result = Exec.execResult(Bob.getExe(curr_platform, "oggz-validate"),
            tmpOggFile.getAbsolutePath()
        );

        if (result.ret != 0) {
            throw new CompileExceptionError(input, 0, 
                String.format("\nSound file validation failed. Make sure your `ogg` files are correct using `oggz-validate` https://www.xiph.org/oggz/\n%s", new String(result.stdOutErr)));
        }

        return super.create(input);
    }

}
