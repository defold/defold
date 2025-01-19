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

import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;

public class ShaderProgramBuilderBundle extends Builder {
    public static final String EXT = ".shbundle";
    public static final String EXT_OUT = ".shbundlec";

    public static class  ModuleBundle implements Serializable {
        private static final long serialVersionUID = 1L;
        private ArrayList<String> modules = new ArrayList<>();

        public void add(String path) {
            modules.add(path);
        }

        public String[] get() {
            return modules.toArray(new String[0]);
        }

        public byte[] toByteArray() throws IOException {
            ByteArrayOutputStream bos = new ByteArrayOutputStream(128 * 16);
            ObjectOutputStream os = new ObjectOutputStream(bos);
            os.writeObject(this);
            os.close();
            return bos.toByteArray();
        }

        public String toBase64String() throws IOException {
            return Base64.getEncoder().encodeToString(toByteArray());
        }

        public static ModuleBundle fromBase64String(String content) throws IOException, ClassNotFoundException {
            byte[] data = Base64.getDecoder().decode(content);
            ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(data));
            return (ModuleBundle) ois.readObject();
        }

        private static ModuleBundle loadFromBytes(byte[] content) {
            try {
                ObjectInputStream is = new ObjectInputStream(new ByteArrayInputStream(content));
                return (ModuleBundle) is.readObject();
            } catch (Throwable ignored) {}
            return null;
        }

        private static ModuleBundle loadFromBase64Bytes(byte[] content) {
            String contentB64String = new String(content, StandardCharsets.UTF_8);
            try {
                return fromBase64String(contentB64String);
            } catch (Throwable ignored) {}
            return null;
        }

        public static ModuleBundle load(IResource resource) throws IOException, CompileExceptionError {
            byte[] content = resource.getContent();

            if (content == null) {
                return new ModuleBundle();
            } else {

                ModuleBundle fromBytes = loadFromBytes(content);
                if (fromBytes != null) {
                    return fromBytes;
                }

                ModuleBundle fromB64Bytes = loadFromBase64Bytes(content);
                if (fromB64Bytes != null) {
                    return fromB64Bytes;
                }

                throw new CompileExceptionError("Unable to load module bundle");
            }
        }
    }

    public static ModuleBundle createBundle() {
        return new ModuleBundle();
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        // I don't think this should happen?
        System.out.println("\nShaderProgramBuilderBundle.create");
        System.out.println(input.getPath());

        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        System.out.println("ShaderProgramBuilderBundle.build");
        System.out.println(task.firstInput().getPath());

        IResource vsRes = task.input(0);
        IResource fsRes = task.input(1);

        BuilderUtil.checkResource(this.project, vsRes, "Vertex program", vsRes.getPath());
        BuilderUtil.checkResource(this.project, fsRes, "Fragment program", fsRes.getPath());

        ModuleBundle modules = new ModuleBundle();
        modules.add(vsRes.getPath());
        modules.add(fsRes.getPath());
        task.output(0).setContent(modules.toByteArray());
    }
}
