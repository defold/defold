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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.bob.util.GameObjectUtil;
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

@BuilderParams(name = "GameObject", inExts = ".go", outExt = ".goc")
public class GameObjectBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder b = GameObjectUtil.loadPrototype(input);

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        for (ComponentDesc cd : b.getComponentsList()) {
            Collection<String> resources = PropertiesUtil.getPropertyDescResources(project, cd.getPropertiesList());
            for(String r : resources) {
                IResource resource = BuilderUtil.checkResource(this.project, input, "resource", r);
                taskBuilder.addInput(resource);
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

                Task<?> embedTask = project.buildResource(genResource);
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
        PrototypeDesc.Builder protoBuilder = GameObjectUtil.loadPrototype(input);
        for (ComponentDesc c : protoBuilder.getComponentsList()) {
            String component = c.getComponent();
            BuilderUtil.checkResource(this.project, input, "component", component);
        }

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

            ComponentDesc c = ComponentDesc.newBuilder()
                    .setId(ec.getId())
                    .setPosition(ec.getPosition())
                    .setRotation(ec.getRotation())
                    .setComponent(path)
                    .build();

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

    static String[][] extensionMapping = new String[][] {
        {".camera", ".camerac"},
        {".buffer", ".bufferc"},
        {".mesh", ".meshc"},
        {".collectionproxy", ".collectionproxyc"},
        {".collisionobject", ".collisionobjectc"},
        {".emitter", ".emitterc"},
        {".particlefx", ".particlefxc"},
        {".gui", ".guic"},
        {".model", ".modelc"},
        {".script", ".scriptc"},
        {".sound", ".soundc"},
        {".wav", ".soundc"},
        {".collectionfactory", ".collectionfactoryc"},
        {".factory", ".factoryc"},
        {".light", ".lightc"},
        {".label", ".labelc"},
        {".sprite", ".spritec"},
        {".tilegrid", ".tilemapc"},
        {".tilemap", ".tilemapc"},
    };

    private PrototypeDesc.Builder transformGo(IResource resource,
            PrototypeDesc.Builder protoBuilder) throws CompileExceptionError {

        protoBuilder.clearPropertyResources();
        Collection<String> propertyResources = new HashSet<String>();
        List<ComponentDesc> newList = new ArrayList<ComponentDesc>();
        for (ComponentDesc cd : protoBuilder.getComponentsList()) {
            String c = cd.getComponent();
            for (String[] fromTo : extensionMapping) {
                c = BuilderUtil.replaceExt(c, fromTo[0], fromTo[1]);
            }

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
