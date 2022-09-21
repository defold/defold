// Copyright 2020-2022 The Defold Foundation
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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Vector4;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.dynamo.gamesys.proto.Label.LabelDesc;
import com.dynamo.proto.DdfMath.Vector3;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "GameObject", inExts = ".go", outExt = ".goc")
public class GameObjectBuilder extends Builder<Void> {


    private boolean isComponentOfType(EmbeddedComponentDesc d, String type) {
        return d.getType().equals(type);
    }
    private boolean isComponentOfType(ComponentDesc d, String type) {
        return FilenameUtils.getExtension(d.getComponent()).equals(type);
    }

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

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        for (ComponentDesc cd : b.getComponentsList()) {
            Collection<String> resources = PropertiesUtil.getPropertyDescResources(project, cd.getPropertiesList());
            for(String r : resources) {
                IResource resource = BuilderUtil.checkResource(this.project, input, "resource", r);
                taskBuilder.addInput(resource);
                PropertiesUtil.createResourcePropertyTasks(this.project, resource, input);
            }
        }

        PrototypeDesc proto = b.build();

        // Gather the unique resources first
        Map<Long, IResource> uniqueResources = new HashMap<>();
        Set<Long> productsOfThisTask = new HashSet<>();
        List<Task<?>> embedTasks = new ArrayList<>();

        for (EmbeddedComponentDesc ec : proto.getEmbeddedComponentsList()) {
            byte[] data = ec.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource genResource = project.getGeneratedResource(hash, ec.getType());
            if (genResource == null) {
                genResource = project.createGeneratedResource(hash, ec.getType());

                // TODO: This is a hack derived from the same problem with embedded gameobjects from collections (see CollectionBuilder.create)!
                // If the file isn't created here <EmbeddedComponent>#create
                // can't access generated resource data (embedded component desc)
                genResource.setContent(data);

                uniqueResources.put(hash, genResource);
                productsOfThisTask.add(hash);
            }
        }

        for (long hash : uniqueResources.keySet()) {
            IResource genResource = uniqueResources.get(hash);

            taskBuilder.addOutput(genResource);

            if (productsOfThisTask.contains(hash)) {

                Task<?> embedTask = project.createTask(genResource);
                if (embedTask == null) {
                    throw new CompileExceptionError(input,
                                                    0,
                                                    String.format("Failed to create build task for component '%s'", genResource.getPath()));
                }
                embedTasks.add(embedTask);
            }
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
            BuilderUtil.checkResource(this.project, input, "component", component);
        }

        final Vector3 defaultScale = Vector3.newBuilder().setX(1.0f).setY(1.0f).setZ(1.0f).build();

        // convert embedded components to generated components in the build folder
        for (EmbeddedComponentDesc ec : protoBuilder.getEmbeddedComponentsList()) {
            if (ec.getId().length() == 0) {
                throw new CompileExceptionError(input, 0, "missing required field 'id'");
            }

            byte[] data = ec.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource genResource = project.getGeneratedResource(hash, ec.getType());

            // TODO: We have to set content again here as distclean might have removed everything at this point (according to CollectionBuilder.java)
            genResource.setContent(data);

            int buildDirLen = project.getBuildDirectory().length();
            String path = genResource.getPath().substring(buildDirLen);

            ComponentDesc.Builder b = ComponentDesc.newBuilder()
                    .setId(ec.getId())
                    .setPosition(ec.getPosition())
                    .setRotation(ec.getRotation())
                    .setComponent(path);

            // #3981
            // copy the scale from the embedded component only if it has a scale to
            // avoid that the component gets the default value for v3 which is 0
            if (ec.hasScale()) {
                b.setScale(ec.getScale());
            }

            ComponentDesc c = b.build();

            protoBuilder.addComponents(c);
        }

        protoBuilder = transformGo(input, protoBuilder);
        protoBuilder.clearEmbeddedComponents();

        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        PrototypeDesc proto = protoBuilder.build();
        proto.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

    private PrototypeDesc.Builder transformGo(IResource resource,
            PrototypeDesc.Builder protoBuilder) throws CompileExceptionError {

        final Vector3 defaultScale = Vector3.newBuilder().setX(1.0f).setY(1.0f).setZ(1.0f).build();

        protoBuilder.clearPropertyResources();
        Collection<String> propertyResources = new HashSet<String>();
        List<ComponentDesc> newList = new ArrayList<ComponentDesc>();
        for (ComponentDesc cd : protoBuilder.getComponentsList()) {
            String c = cd.getComponent();
            // Use the BuilderParams from each builder to map the input ext to the output ext
            String inExt = "." + FilenameUtils.getExtension(c);
            String outExt = project.replaceExt(inExt);
            c = BuilderUtil.replaceExt(c, inExt, outExt);

            PropertyDeclarations.Builder properties = PropertyDeclarations.newBuilder();
            for (PropertyDesc desc : cd.getPropertiesList()) {
                if (!PropertiesUtil.transformPropertyDesc(project, desc, properties, propertyResources)) {
                    throw new CompileExceptionError(resource, 0, String.format("The property %s.%s has an invalid format: %s", cd.getId(), desc.getId(), desc.getValue()));
                }
            }
            ComponentDesc.Builder compBuilder = ComponentDesc.newBuilder(cd);

            // #3981
            // migrate label scale from LabelDesc to ComponentDesc
            if (isComponentOfType(cd, "label")) {
                try {
                    // find the label resource in one of two places:
                    // * in the build directory if it is generated from an embedded label (see build step above)
                    // * in the project
                    IResource labelResource = project.getResource(cd.getComponent());
                    if (!labelResource.exists()) {
                        labelResource = labelResource.output();
                    }
                    FileReader reader = new FileReader(labelResource.getAbsPath());
                    LabelDesc.Builder lb = LabelDesc.newBuilder();
                    TextFormat.merge(reader, lb);
                    if (lb.hasScale()) {
                        Vector4 labelScaleV4 = lb.getScale();
                        Vector3 labelScaleV3 = Vector3.newBuilder().setX(labelScaleV4.getX()).setY(labelScaleV4.getY()).setZ(labelScaleV4.getZ()).build();
                        compBuilder.setScale(labelScaleV3);
                    }
                }
                catch(IOException e) {
                    throw new CompileExceptionError(e);
                }
            }

            // #3981
            // if the component doesn't have a scale we set a default of v3(1.0)
            // if we do not set a scale it will have a scale of v3(0.0) at runtime
            if (!compBuilder.hasScale()) {
                compBuilder.setScale(defaultScale);
            }

            compBuilder.setComponent(c);
            compBuilder.setPropertyDecls(properties);
            newList.add(compBuilder.build());
        }
        protoBuilder.addAllPropertyResources(propertyResources);
        protoBuilder.clearComponents();
        protoBuilder.addAllComponents(newList);

        return protoBuilder;
    }

}
