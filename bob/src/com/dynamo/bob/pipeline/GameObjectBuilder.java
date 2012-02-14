package com.dynamo.bob.pipeline;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "GameObject", inExts = ".go", outExt = ".goc")
public class GameObjectBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException {
        PrototypeDesc.Builder b = PrototypeDesc.newBuilder();
        TextFormat.merge(new String(input.getContent()), b);
        PrototypeDesc proto = b.build();

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        int i = 0;
        String name = FilenameUtils.getBaseName(input.getPath());
        List<Task<?>> embedTasks = new ArrayList<Task<?>>();
        for (EmbeddedComponentDesc ec : proto.getEmbeddedComponentsList()) {

            String embedName = String.format("%s_generated_%d.%s", name, i, ec.getType());
            IResource genResource = input.getResource(embedName).output();
            taskBuilder.addOutput(genResource);

            Task<?> embedTask = project.buildResource(genResource);
            embedTasks.add(embedTask);
            ++i;
        }

        Task<Void> task = taskBuilder.build();
        for (Task<?> et : embedTasks) {
            et.setProductOf(task);
        }

        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {
        IResource input = task.getInputs().get(0);

        PrototypeDesc.Builder protoBuilder = PrototypeDesc.newBuilder();
        TextFormat.merge(new String(input.getContent()), protoBuilder);

        for (ComponentDesc c : protoBuilder.getComponentsList()) {
            IResource r = project.getResource(c.getComponent().substring(1));
            if (!r.exists()) {
                String msg = String.format("%s:0: error: is missing dependent resource file '%s'", input.getPath(), c.getComponent());;
                throw new CompileExceptionError(msg);
            }
        }

        int i = 0;
        for (EmbeddedComponentDesc ec : protoBuilder.getEmbeddedComponentsList()) {
            if (ec.getId().length() == 0) {
                throw new CompileExceptionError("missing required field 'id'");
            }

            task.output(i+1).setContent(ec.getData().getBytes());

            int buildDirLen = project.getBuildDirectory().length();
            String path = task.output(i+1).getPath().substring(buildDirLen);

            ComponentDesc c = ComponentDesc.newBuilder()
                    .setId(ec.getId())
                    .setPosition(ec.getPosition())
                    .setRotation(ec.getRotation())
                    .setComponent(path)
                    .build();

            protoBuilder.addComponents(c);
            ++i;
        }

        protoBuilder = transformGo(protoBuilder);
        protoBuilder.clearEmbeddedComponents();

        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        PrototypeDesc proto = protoBuilder.build();
        proto.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

    static String[][] extensionMapping = new String[][] {
        {".camera", ".camerac"},
        {".collectionproxy", ".collectionproxyc"},
        {".collisionobject", ".collisionobjectc"},
        {".emitter", ".emitterc"},
        {".gui", ".guic"},
        {".model", ".modelc"},
        {".script", ".scriptc"},
        {".wav", ".wavc"},
        {".spawnpoint", ".spawnpointc"},
        {".light", ".lightc"},
        // Temp rename for sprite2. Otherwise the rule .sprite -> .spritec would replace
        // Using replace is not entirely correct. We should just fix replaceExt...
        {".sprite2", ".dummysprite2c"},
        {".sprite", ".spritec"},
        {".dummysprite2c", ".sprite2c"},
        {".tileset", ".tilesetc"},
        {".tilegrid", ".tilegridc"},
    };

    private PrototypeDesc.Builder transformGo(
            PrototypeDesc.Builder protoBuilder) {


        List<ComponentDesc> newList = new ArrayList<ComponentDesc>();
        for (ComponentDesc cd : protoBuilder.getComponentsList()) {
            String c = cd.getComponent();
            for (String[] fromTo : extensionMapping) {
                c = BuilderUtil.replaceExt(c, fromTo[0], fromTo[1]);
            }
            newList.add(ComponentDesc.newBuilder().mergeFrom(cd).setComponent(c).build());
        }
        protoBuilder.clearComponents();
        protoBuilder.addAllComponents(newList);

        return protoBuilder;
    }

}
