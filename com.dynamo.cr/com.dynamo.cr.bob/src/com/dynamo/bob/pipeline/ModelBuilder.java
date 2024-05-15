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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Logger;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.gamesys.proto.ModelProto.Material;
import com.dynamo.gamesys.proto.ModelProto.Model;
import com.dynamo.gamesys.proto.ModelProto.ModelDesc;
import com.dynamo.gamesys.proto.ModelProto.Texture;
import com.dynamo.graphics.proto.Graphics.VertexAttribute;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.rig.proto.Rig.RigScene;
import com.google.protobuf.TextFormat;


@BuilderParams(name="Model", inExts=".model", outExt=".modelc")
public class ModelBuilder extends Builder<Void> {

    private static Logger logger = Logger.getLogger(ModelBuilder.class.getName());

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        ModelDesc.Builder modelDescBuilder = ModelDesc.newBuilder();
        ProtoUtil.merge(input, modelDescBuilder);

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".rigscenec"));

        IResource mesh = BuilderUtil.checkResource(this.project, input, "mesh", modelDescBuilder.getMesh());
        taskBuilder.addInput(mesh);
        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            IResource skeleton = BuilderUtil.checkResource(this.project, input, "skeleton", modelDescBuilder.getSkeleton());
            taskBuilder.addInput(skeleton);
        }
        // Check if it's an individual file or not
        if((!modelDescBuilder.getAnimations().isEmpty()) && !modelDescBuilder.getAnimations().endsWith(".animationset")) {
            IResource animations = BuilderUtil.checkResource(this.project, input, "animation", modelDescBuilder.getAnimations());
            taskBuilder.addInput(animations);
        }

        if (modelDescBuilder.getMaterialsCount() > 0) {
            for (Material material : modelDescBuilder.getMaterialsList()) {
                for (Texture texture : material.getTexturesList()) {
                    String t = texture.getTexture();
                    if (t.isEmpty())
                        continue; // TODO: Perhaps we can check if the material expects textures?

                    IResource res = BuilderUtil.checkResource(this.project, input, "texture", t);
                    Task<?> embedTask = this.project.createTask(res);
                    if (embedTask == null) {
                        throw new CompileExceptionError(input,
                                                        0,
                                                        String.format("Failed to create build task for component '%s'", res.getPath()));
                    }
                }

                IResource materialOutput = this.project.getResource(material.getMaterial()).changeExt(".materialc");
                taskBuilder.addInput(materialOutput);
            }
        } else {
            // Deprecated workflow
            for (String t : modelDescBuilder.getTexturesList()) {
                if (t.isEmpty())
                    continue; // TODO: Perhaps we can check if the material expects textures?

                IResource res = BuilderUtil.checkResource(this.project, input, "texture", t);
                Task<?> embedTask = this.project.createTask(res);
                if (embedTask == null) {
                    throw new CompileExceptionError(input,
                                                    0,
                                                    String.format("Failed to create build task for component '%s'", res.getPath()));
                }
            }

            if (!modelDescBuilder.getMaterial().isEmpty()) {
                IResource materialOutput = this.project.getResource(modelDescBuilder.getMaterial()).changeExt(".materialc");
                taskBuilder.addInput(materialOutput);
            }
        }

        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayInputStream model_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader model_isr = new InputStreamReader(model_is);
        ModelDesc.Builder modelDescBuilder = ModelDesc.newBuilder();
        TextFormat.merge(model_isr, modelDescBuilder);

        // Rigscene
        RigScene.Builder rigBuilder = RigScene.newBuilder();
        rigBuilder.setMeshSet(BuilderUtil.replaceExt(modelDescBuilder.getMesh(), ".meshsetc"));
        if(!modelDescBuilder.getSkeleton().isEmpty()) {
            rigBuilder.setSkeleton(BuilderUtil.replaceExt(modelDescBuilder.getSkeleton(), ".skeletonc"));
        }

        if (modelDescBuilder.getAnimations().equals("")) {
            // No animations
        }
        else if(modelDescBuilder.getAnimations().endsWith(".animationset")) {
            // if an animsetdesc file is animation input, use animations and skeleton from that file(s) and other related data (weights, boneindices..) from the mesh collada file
            rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), ".animationsetc"));
        }
        else if(!modelDescBuilder.getAnimations().isEmpty()) {
            // if a collada file is animation input, use animations from that file and other related data (weights, boneindices..) from the mesh collada file
            // we define this a generated file as the animation set does not come from an .animationset resource, but is exported directly from this collada file
            // and because we also avoid possible resource name collision (ref: atlas <-> texture).
            rigBuilder.setAnimationSet(BuilderUtil.replaceExt(modelDescBuilder.getAnimations(), "_generated_0.animationsetc"));
        } else {
            throw new CompileExceptionError(task.input(0), -1, "No animation set in model!");
        }

        rigBuilder.setTextureSet(""); // this is set in the model
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        rigBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // Model
        IResource resource = task.input(0);
        Model.Builder model = Model.newBuilder();
        model.setRigScene(task.output(1).getPath().replace(this.project.getBuildDirectory(), ""));

        if (modelDescBuilder.getMaterialsCount() > 0)
        {
            for (Material material : modelDescBuilder.getMaterialsList()) {
                Material.Builder materialBuilder = Material.newBuilder();

                IResource materialSourceResource = BuilderUtil.checkResource(this.project, resource, "material", material.getMaterial());
                materialBuilder.setName(material.getName());
                materialBuilder.setMaterial(BuilderUtil.replaceExt(material.getMaterial(), ".material", ".materialc"));

                List<Texture> texturesList = new ArrayList<>();
                for (Texture t : material.getTexturesList()) {
                    BuilderUtil.checkResource(this.project, resource, "texture", t.getTexture());

                    Texture.Builder textureBuilder = Texture.newBuilder(t); // create from existing message
                    textureBuilder.setTexture(ProtoBuilders.replaceTextureName(t.getTexture()));
                    texturesList.add(textureBuilder.build());
                }

                materialBuilder.addAllTextures(texturesList);

                IResource materialBuildResource            = materialSourceResource.changeExt(".materialc");
                MaterialDesc.Builder materialSourceBuilder = MaterialDesc.newBuilder();
                materialSourceBuilder.mergeFrom(materialBuildResource.getContent());

                List<VertexAttribute> materialAttributes      = materialSourceBuilder.getAttributesList();
                List<VertexAttribute> modelAttributeOverrides = new ArrayList<VertexAttribute>();

                for (int i=0; i < material.getAttributesCount(); i++) {
                    VertexAttribute modelAttribute    = material.getAttributes(i);
                    VertexAttribute materialAttribute = GraphicsUtil.getAttributeByName(materialAttributes, modelAttribute.getName());

                    if (materialAttribute != null) {
                        modelAttributeOverrides.add(GraphicsUtil.buildVertexAttribute(modelAttribute, materialAttribute));
                    }
                }

                materialBuilder.addAllAttributes(modelAttributeOverrides);

                model.addMaterials(materialBuilder);
            }
        } else {
            // Deprecated workflow
            String singleMaterial = modelDescBuilder.getMaterial();
            if (!singleMaterial.isEmpty()) {

                // TODO: Handle migration in the next MVP
                // logger.log(Level.WARNING, String.format("Model %s uses deprecated material format. Please resave in the editor!", task.input(0).getAbsPath()));

                BuilderUtil.checkResource(this.project, resource, "material", singleMaterial);

                Material.Builder materialBuilder = Material.newBuilder();
                materialBuilder.setName("default");
                materialBuilder.setMaterial(BuilderUtil.replaceExt(singleMaterial, ".material", ".materialc"));

                List<Texture> texturesList = new ArrayList<>();
                for (String t : modelDescBuilder.getTexturesList()) {
                    if (t.isEmpty())
                        continue; // TODO: Perhaps we can check if the material expects textures?

                    BuilderUtil.checkResource(this.project, resource, "texture", t);

                    Texture.Builder textureBuilder = Texture.newBuilder();
                    textureBuilder.setSampler(""); // If they have no name, then we detect that an treat it as an array
                    textureBuilder.setTexture(ProtoBuilders.replaceTextureName(t));
                    texturesList.add(textureBuilder.build());
                }

                materialBuilder.addAllTextures(texturesList);

                model.addMaterials(materialBuilder);
            }
        }

        model.setDefaultAnimation(modelDescBuilder.getDefaultAnimation());

        out = new ByteArrayOutputStream(64 * 1024);
        model.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
