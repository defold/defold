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
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "GameObject", inExts = ".go", outExt = ".goc")
public class GameObjectBuilder extends Builder<Void> {

    private PrototypeDesc.Builder loadPrototype(IResource input) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder b = PrototypeDesc.newBuilder();
        ProtoUtil.merge(input, b);

        List<ComponentDesc> lst = b.getComponentsList();
        List<ComponentDesc> newList = new ArrayList<GameObject.ComponentDesc>();


        for (ComponentDesc componentDesc : lst) {
            // Convert .wav resource component to an embedded sound
            // We might generalize this in the future if necessary
            if (componentDesc.getComponent().endsWith(".wav")) {
                SoundDesc.Builder sd = SoundDesc.newBuilder().setSound(componentDesc.getComponent());
                EmbeddedComponentDesc ec = EmbeddedComponentDesc.newBuilder()
                    .setId(componentDesc.getId())
                    .setType("sound")
                    .setData(TextFormat.printToString(sd.build()))
                    .build();
                b.addEmbeddedComponents(ec);
            } else {
                newList.add(componentDesc);
            }
        }

        b.clearComponents();
        b.addAllComponents(newList);


        return b;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder b = loadPrototype(input);
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
            if (embedTask == null) {
                throw new CompileExceptionError(input,
                                                0,
                                                String.format("Failed to create build task for component '%s'", ec.getId()));
            }
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

        PrototypeDesc.Builder protoBuilder = loadPrototype(input);
        for (ComponentDesc c : protoBuilder.getComponentsList()) {
            String component = c.getComponent();
            BuilderUtil.checkFile(this.project, input, "component", component);
        }

        int i = 0;
        for (EmbeddedComponentDesc ec : protoBuilder.getEmbeddedComponentsList()) {
            if (ec.getId().length() == 0) {
                throw new CompileExceptionError(input, 0, "missing required field 'id'");
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
        {".particlefx", ".particlefxc"},
        {".gui", ".guic"},
        {".model", ".modelc"},
        {".script", ".scriptc"},
        {".sound", ".soundc"},
        {".wav", ".soundc"},
        {".factory", ".factoryc"},
        {".light", ".lightc"},
        {".sprite", ".spritec"},
        {".tilegrid", ".tilegridc"},
        {".tilemap", ".tilegridc"},
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
