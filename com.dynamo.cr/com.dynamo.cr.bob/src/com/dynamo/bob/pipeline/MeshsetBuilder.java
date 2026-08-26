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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.TextureUtil;

import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.UncompactedMeshSet;


@BuilderParams(name="Meshset", inExts={".gltf",".glb"}, outExt=".meshsetc", paramsForSignature = {
        "model-split-large-meshes",
        "model-max-morph-target-texture-width",
        "model-max-morph-target-texture-height"})
public class MeshsetBuilder extends Builder  {
    static final String UNCOMPACTED_MESHSET_EXT = "_uncompacted.meshsetbuildc";

    /**
     * Bridges ModelUtil's resource-agnostic model loading with Bob's task output
     * layout. ModelUtil packs morph target textures while building meshes, and
     * this collector allocates the corresponding task output and returns the
     * resource path that should be stored in the meshset.
     */
    private static class MeshSetMorphTargetTextureCollector extends ModelUtil.MorphTargetTextureCollector {
        private final Project project;
        private final Task task;
        private final ArrayList<IResource> outputs = new ArrayList<>();

        public MeshSetMorphTargetTextureCollector(Project project, Task task) {
            this.project = project;
            this.task = task;
        }

        public ArrayList<IResource> getOutputs() {
            return outputs;
        }

        @Override
        protected String getMorphTargetTextureResourcePath(int index, ModelUtil.PackedMorphTargetTexture texture) {
            // The first four task outputs are meshsetc, skeletonc,
            // animationsetc and the build-only uncompacted meshset.
            int outputIndex = 4 + index;
            IResource output = task.output(outputIndex);
            if (output == null) {
                throw new IllegalStateException(String.format(
                        "Morph texture output %d was not declared during task creation", index));
            }
            outputs.add(output);
            return BuilderUtil.getRelativePath(project, output);
        }
    }

    private static void writeUncompactedMeshSet(Task task, List<Rig.Model> models) throws IOException {
        UncompactedMeshSet.Builder builder = UncompactedMeshSet.newBuilder();
        builder.addAllModels(models);
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        builder.build().writeTo(out);
        out.close();
        task.output(3).setContent(out.toByteArray());
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
            .setName(params.name())
            .addInput(input)
            .addOutput(input.changeExt(params.outExt()))
            .addOutput(input.changeExt(".skeletonc"))
            .addOutput(input.changeExt("_generated_0.animationsetc"))
            .addOutput(input.changeExt(UNCOMPACTED_MESHSET_EXT));

        byte[] sceneContent;
        GltfResourceUtil.Metadata metadata;
        try {
            sceneContent = input.getContent();
            if (sceneContent == null) {
                throw new IOException(String.format("glTF resource '%s' has no content", input.getPath()));
            }
            metadata = GltfResourceUtil.scan(sceneContent, input.getPath());
            GltfResourceUtil.addExternalBufferInputs(input, taskBuilder, metadata);
        } catch (IOException e) {
            // Defer malformed glTF diagnostics to build(), where the importer and validator
            // provide the existing detailed error messages.
            return taskBuilder.build();
        }

        int morphTargetTextureCount = 0;
        if (metadata.hasMorphTargets()) {
            Modelimporter.Scene scene = null;
            try {
                scene = ModelUtil.loadScene(sceneContent, input.getPath(), new Modelimporter.Options(),
                        new GltfResourceUtil.ResourceDataResolver(input));
                ModelUtil.releaseSceneBuffers(scene);
                if (this.project.option("model-split-large-meshes", "false").equals("true")) {
                    ModelUtil.splitMeshes(scene);
                }
                morphTargetTextureCount = ModelUtil.getNumMorphTargetTextures(scene);
            } catch (IOException e) {
                // Defer import and validation errors to build(), where existing glTF
                // diagnostics are reported.
            } finally {
                if (scene != null) {
                    ModelUtil.unloadScene(scene);
                }
            }
        }

        for (int i = 0; i < morphTargetTextureCount; ++i) {
            taskBuilder.addOutput(input.changeExt(String.format("_morph_%d.texturec", i)));
        }
        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {

        String suffix = BuilderUtil.getSuffix(task.input(0).getPath());

        if (suffix.equals("gltf") || suffix.equals("glb")) {
            validateGltf(task, suffix);
        }

        BobProjectProperties projectProperties = this.project.getProjectProperties();
        int morphTexW = projectProperties.getIntValue("model", "max_morph_target_texture_width", 1024);
        int morphTexH = projectProperties.getIntValue("model", "max_morph_target_texture_height", 1024);

        Modelimporter.Options options = new Modelimporter.Options();
        GltfResourceUtil.ResourceDataResolver dataResolver = new GltfResourceUtil.ResourceDataResolver(task.input(0));
        Modelimporter.Scene scene;
        try {
            scene = ModelUtil.loadScene(task.input(0).getContent(), task.input(0).getPath(), options, dataResolver);
        } catch (IOException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }
        ModelUtil.releaseSceneBuffers(scene);

        try {
            boolean split_meshes = this.project.option("model-split-large-meshes", "false").equals("true");

            // Build-only source geometry for collision compilation. Keeping it
            // in a separate output prevents runtime model selection from
            // bypassing the compact render meshes.
            writeUncompactedMeshSet(task, split_meshes
                    ? ModelUtil.preserveUnsplitRawModels(scene)
                    : Collections.emptyList());

            if (split_meshes) {
                ModelUtil.splitMeshes(scene);
            }

            // MeshSet
            {
                MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
                MeshSetMorphTargetTextureCollector morphTextureCollector = new MeshSetMorphTargetTextureCollector(project, task);

                try {
                    ModelUtil.loadModels(scene, meshSetBuilder, morphTexW, morphTexH, morphTextureCollector);
                } catch (LoaderException | IllegalStateException e) {
                    throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
                }

                int expectedMorphTextureCount = task.getOutputs().size() - 4;
                int actualMorphTextureCount = morphTextureCollector.getTextures().size();
                if (actualMorphTextureCount != expectedMorphTextureCount) {
                    throw new CompileExceptionError(task.input(0), -1, String.format(
                            "glTF task declared %d morph texture outputs but generated %d",
                            expectedMorphTextureCount, actualMorphTextureCount));
                }

                ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
                meshSetBuilder.build().writeTo(out);
                out.close();
                task.output(0).setContent(out.toByteArray());

                ArrayList<ModelUtil.CollectedMorphTargetTexture> morphTargetTextures = morphTextureCollector.getTextures();
                ArrayList<IResource> morphTargetTextureOutputs = morphTextureCollector.getOutputs();
                for (int i = 0; i < morphTargetTextures.size(); ++i) {
                    TextureUtil.writeGenerateResultToResource(morphTargetTextures.get(i).texture.toGenerateResult(), morphTargetTextureOutputs.get(i));
                }
            }

            // Skeleton
            {
                Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
                if (ModelUtil.getNumSkins(scene) > 0)
                {
                    if (!ModelUtil.loadSkeleton(scene, skeletonBuilder))
                    {
                        throw new CompileExceptionError(task.input(0), -1, "Failed to load skeleton");
                    }
                }

                ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
                skeletonBuilder.build().writeTo(out);
                out.close();
                task.output(1).setContent(out.toByteArray());
            }

            // Animationset
            {
                AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
                if (ModelUtil.getNumAnimations(scene) > 0) {
                    ModelUtil.loadAnimations(scene, animationSetBuilder, "", new ArrayList<String>());
                }

                ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
                animationSetBuilder.build().writeTo(out);
                out.close();
                task.output(2).setContent(out.toByteArray());
            }
        } finally {
            ModelUtil.unloadScene(scene);
        }
    }

    private static boolean isPhysicalFile(IResource resource) {
        return new File(resource.getAbsPath()).isFile();
    }

    private void validateGltf(Task task, String suffix) throws CompileExceptionError {
        IResource input = task.input(0);
        try {

            GLTFValidator.ValidateResult validateResult;

            // NOTE: If the file is part of an archive, we cannot validate embedded or external resources
            if (isPhysicalFile(input)) {
                validateResult = GLTFValidator.validateGltf(input.getAbsPath(), true);
            } else {
                validateResult = GLTFValidator.validateGltf(input.getContent(), suffix, false);
            }

            if (!validateResult.result()) {
                StringBuilder sb = new StringBuilder();
                sb.append("Errors reported by gltf_validator:\n");

                for (GLTFValidator.ValidateError err : validateResult.errors()) {
                    String line = String.format(" - %s (pointer: %s, code: %s)\n", err.message(), err.pointer(), err.code());
                    sb.append(line);
                }

                throw new CompileExceptionError(input, 0, sb.toString());
            }
        } catch (IOException exc) {
            throw new CompileExceptionError(input, 0,
                    String.format("Failed to run glTF validator: %s", exc.getMessage()));
        }
    }
}
