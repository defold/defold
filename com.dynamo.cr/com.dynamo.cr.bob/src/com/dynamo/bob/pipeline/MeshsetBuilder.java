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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.xml.stream.XMLStreamException;

import org.apache.commons.io.FilenameUtils;

import java.io.IOException;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParseException;
import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;


@BuilderParams(name="Meshset", inExts={".dae",".gltf",".glb"}, outExt=".meshsetc", paramsForSignature = {"model-split-large-meshes"})
public class MeshsetBuilder extends Builder  {
    private static String gltfValidatorExePath;
    public static class ResourceDataResolver implements ModelImporterJni.DataResolver
    {
        Project project;

        public ResourceDataResolver(Project project) {
            this.project = project;
        }

        public byte[] getData(String path, String uri) {
            File file = new File(path);
            File bufferFile = new File(file.getParentFile(), uri);
            IResource resource = project.getResource(bufferFile.getPath());
            if (resource == null)
            {
                System.out.printf("Failed to find data for %s\n", bufferFile.getPath());
                return null;
            }
            try {
                return resource.getContent();
            } catch (IOException e) {
                return null; // Actual errors are reported by ModeulUtil.loadScene
            }
        }
    };

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
            .setName(params.name())
            .addInput(input)
            .addOutput(input.changeExt(params.outExt()))
            .addOutput(input.changeExt(".skeletonc"))
            .addOutput(input.changeExt("_generated_0.animationsetc"));
        return taskBuilder.build();
    }

    public void buildCollada(Task task) throws CompileExceptionError, IOException {
        // Previously ColladaModelBuilder.java
        ByteArrayInputStream collada_is = new ByteArrayInputStream(task.input(0).getContent());

        // MeshSet
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();

        boolean split_meshes = this.project.option("model-split-large-meshes", "false").equals("true");
        try {
            ColladaUtil.loadMesh(collada_is, meshSetBuilder, true, split_meshes);
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile mesh: " + e.getLocalizedMessage(), e);
        }
        meshSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());

        // Skeleton
        out = new ByteArrayOutputStream(64 * 1024);
        collada_is.reset();
        Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
        try {
            ColladaUtil.loadSkeleton(collada_is, skeletonBuilder, new ArrayList<String>());
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile skeleton: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile skeleton: " + e.getLocalizedMessage(), e);
        }
        skeletonBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // Animationset
        out = new ByteArrayOutputStream(64 * 1024);
        collada_is.reset();
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
        try {
            ColladaUtil.loadAnimations(collada_is, animationSetBuilder, FilenameUtils.getBaseName(task.input(0).getPath()), new ArrayList<String>());
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile animation: " + e.getLocalizedMessage(), e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile animation: " + e.getLocalizedMessage(), e);
        }
        animationSetBuilder.build().writeTo(out);
        out.close();
        task.output(2).setContent(out.toByteArray());
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {

        String suffix = BuilderUtil.getSuffix(task.input(0).getPath());

        if (suffix.equals("dae")) {
            buildCollada(task); // Until our model importer supports collada
            return;
        }

        if (suffix.equals("gltf") || suffix.equals("glb")) {
            validateGltf(task, suffix);
        }

        Modelimporter.Options options = new Modelimporter.Options();
        ResourceDataResolver dataResolver = new ResourceDataResolver(this.project);
        Modelimporter.Scene scene = ModelUtil.loadScene(task.input(0).getContent(), task.input(0).getPath(), options, dataResolver);
        if (scene == null) {
            throw new CompileExceptionError(task.input(0), -1, "Error loading model");
        }

        // MeshSet
        {
            MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();

            boolean split_meshes = this.project.option("model-split-large-meshes", "false").equals("true");
            if (split_meshes) {
                ModelUtil.splitMeshes(scene);
            }

            ModelUtil.loadModels(scene, meshSetBuilder);

            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            meshSetBuilder.build().writeTo(out);
            out.close();
            task.output(0).setContent(out.toByteArray());
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

        ModelUtil.unloadScene(scene);
    }

    private void validateGltf(Task task, String suffix) throws CompileExceptionError {
        IResource input = task.input(0);
        File tmpGltfFile = null;

        try {
            tmpGltfFile = File.createTempFile("gltf_tmp", "." + suffix, Bob.getRootFolder());
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(tmpGltfFile));
            try {
                os.write(input.getContent());
            } finally {
                os.close();
            }
        } catch (IOException exc) {
            throw new CompileExceptionError(input, 0,
                    String.format("Cannot copy glTF file to further process: %s", exc.getMessage()));
        }

        try {
            gltfValidatorExePath = Bob.getHostExeOnce("gltf_validator", gltfValidatorExePath);
            Result result = Exec.execResult(gltfValidatorExePath, "-o", tmpGltfFile.getAbsolutePath());

            String output = new String(result.stdOutErr);

            // First, read the human-readable header line to determine if there are any errors.
            // Example: "Errors: 4, Warnings: 0, Infos: 0, Hints: 2"
            int errorCount = 0;
            Matcher headerMatcher = Pattern.compile("Errors:\\s*(\\d+)").matcher(output);
            if (headerMatcher.find()) {
                errorCount = Integer.parseInt(headerMatcher.group(1));
            } else if (result.ret != 0) {
                // If we cannot find the header line but validator returned non-zero,
                // fall back to previous behavior and fail with the full output.
                throw new CompileExceptionError(input, 0,
                        String.format("\nModel validation failed. Make sure your `glTF` files are correct using `gltf_validator`.\n%s", output));
            }

            // If there are no errors reported, accept the file (warnings/infos/hints are allowed).
            if (errorCount == 0) {
                return;
            }

            // There are errors; now locate and parse the JSON payload for detailed messages.
            int jsonStart = output.indexOf('{');
            if (jsonStart == -1) {
                throw new CompileExceptionError(input, 0,
                        String.format("\nModel validation failed. Make sure your `glTF` files are correct using `gltf_validator`.\n%s", output));
            }

            String json = output.substring(jsonStart);

            try {
                ObjectMapper mapper = new ObjectMapper();
                JsonNode root = mapper.readValue(new InputStreamReader(new ByteArrayInputStream(json.getBytes("UTF-8")), "UTF-8"), JsonNode.class);

                JsonNode issues = root.get("issues");
                if (issues == null || !issues.has("messages")) {
                    throw new CompileExceptionError(input, 0,
                            String.format("\nModel validation failed. Make sure your `glTF` files are correct using `gltf_validator`.\n%s", output));
                }

                ArrayList<String> errorMessages = new ArrayList<String>();

                JsonNode messages = issues.get("messages");
                if (messages != null && messages.isArray()) {
                    for (JsonNode msgNode : messages) {
                        JsonNode severityNode = msgNode.get("severity");
                        JsonNode messageNode = msgNode.get("message");
                        if (severityNode == null || messageNode == null) {
                            continue;
                        }

                        int severity = severityNode.asInt();
                        // Severity mapping in gltf_validator:
                        // 0 = error, 1 = warning, 2 = info, 3 = hint.
                        if (severity == 0) {
                            String message = messageNode.asText();
                            JsonNode pointerNode = msgNode.get("pointer");
                            JsonNode codeNode = msgNode.get("code");

                            StringBuilder detail = new StringBuilder();
                            if (pointerNode != null && !pointerNode.asText().isEmpty()) {
                                detail.append("pointer: ").append(pointerNode.asText());
                            }
                            if (codeNode != null && !codeNode.asText().isEmpty()) {
                                if (detail.length() > 0) {
                                    detail.append(", ");
                                }
                                detail.append("code: ").append(codeNode.asText());
                            }

                            if (detail.length() > 0) {
                                message += " (" + detail.toString() + ")";
                            }

                            errorMessages.add(message);
                        }
                    }
                }

                StringBuilder sb = new StringBuilder();
                sb.append("Model validation failed. Make sure your `glTF` files are correct using `gltf_validator`.\n");
                sb.append("Errors reported by gltf_validator:\n");
                for (String msg : errorMessages) {
                    sb.append(" - ").append(msg).append("\n");
                }

                throw new CompileExceptionError(input, 0, sb.toString());
            } catch (JsonParseException e) {
                throw new CompileExceptionError(input, 0,
                        String.format("\nModel validation failed. Make sure your `glTF` files are correct using `gltf_validator`.\n%s", output));
            }
        } catch (IOException exc) {
            throw new CompileExceptionError(input, 0,
                    String.format("Failed to run glTF validator: %s", exc.getMessage()));
        }
    }
}
