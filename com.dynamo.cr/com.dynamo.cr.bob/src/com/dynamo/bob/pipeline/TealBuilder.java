// Copyright 2020-2023 The Defold Foundation
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
import java.io.File;
import java.io.FileInputStream;
import java.nio.file.Path;
import java.nio.file.Files;
import java.util.List;
import java.util.Arrays;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.FileUtil;

@BuilderParams(name = "Teal", inExts = ".tl", outExt = ".lua")
public class TealBuilder extends Builder<Void> {

    private static Logger logger = Logger.getLogger(TealBuilder.class.getName());

    // main/foo.tl:6:33: argument 2: got integer, expected string
    private static Pattern pattern = Pattern.compile("(.*?):(\\d*):(\\d*): (.*)", Pattern.CASE_INSENSITIVE);

    // ========================================
    // 1 error:
    // main/foo.tl:6:33: argument 2: got integer, expected string
    public void check(IResource input) throws IOException, CompileExceptionError {
        Result result = Exec.execResult("tl", "check", input.getAbsPath());
        if (result.ret != 0) {
            String r = new String(result.stdOutErr);
            List<String> lines = Arrays.asList(r.split("\n"));
            String firstError = lines.get(2);
            Matcher matcher = pattern.matcher(firstError);
            boolean matchFound = matcher.find();
            if (matchFound) {
                String filename = matcher.group(1);
                String line = matcher.group(2);
                String column = matcher.group(3);
                String error = matcher.group(4);
                throw new CompileExceptionError(input, Integer.parseInt(line), error);
            }
            else {
                throw new CompileExceptionError(input, 0, r);
            }
        }
    }

    public IResource transpile(IResource input) throws IOException, CompileExceptionError {
        IResource output = input.changeExt(params.outExt());
        Files.createDirectories(new File(output.getAbsPath()).getParentFile().toPath());
        Result result = Exec.execResult("tl", "gen", input.getAbsPath(), "-o", output.getAbsPath());
        if (result.ret != 0) {
            throw new CompileExceptionError(input, 0, String.format("%s", new String(result.stdOutErr)));
        }
        return output;
    }

    public IResource transpile(IResource input, boolean checkInput) throws IOException, CompileExceptionError {
        if (checkInput) {
            check(input);
        }
        return transpile(input);
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {

        LuaBuilder.addLuaSearchPath(project.getBuildDirectory());
        IResource output = transpile(input, true);
        this.project.createTask(output, ScriptBuilders.LuaScriptBuilder.class);

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(output);

        return taskBuilder.build();
    }


    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {}
}
