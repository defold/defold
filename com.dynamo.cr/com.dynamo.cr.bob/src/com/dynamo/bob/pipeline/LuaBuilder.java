package com.dynamo.bob.pipeline;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.List;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.lua.proto.Lua.LuaModule.Type;
import com.google.protobuf.ByteString;
import com.google.protobuf.Message;

/**
 * Lua builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author chmu
 *
 */
public abstract class LuaBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        LuaModule.Builder builder = LuaModule.newBuilder();
        byte[] scriptBytes = task.input(0).getContent();
        String script = new String(scriptBytes);
        List<String> modules = LuaScanner.scan(script);

        for (String module : modules) {
            String module_file = String.format("/%s.lua", module.replaceAll("\\.", "/"));
            BuilderUtil.checkFile(this.project, task.input(0), "module", module_file);
            builder.addModules(module);
            builder.addResources(module_file + "c");
        }
        builder.setType(Type.TYPE_TEXT);
        builder.setScript(ByteString.copyFrom(scriptBytes));
        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}

