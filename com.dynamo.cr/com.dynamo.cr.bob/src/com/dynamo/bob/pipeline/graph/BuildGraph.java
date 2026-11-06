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

package com.dynamo.bob.pipeline.graph;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Collection;
import java.util.EnumSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import org.apache.commons.codec.digest.DigestUtils;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.codehaus.jackson.type.TypeReference;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.liveupdate.PackageBuilder;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultResource;
import com.dynamo.bob.fs.IResource;

/** Compiled resource dependencies and output metadata retained between build and bundle invocations. */
public final class BuildGraph {
    private static final int VERSION = 1;
    private static final ObjectMapper JSON = new ObjectMapper();
    private final ObjectNode data;

    private BuildGraph(ObjectNode data) {
        this.data = data;
    }

    /** Locates the graph alongside game.projectc in this build directory. */
    public static IResource resource(Project project) {
        return project.getGameProjectResource().disableMinifyPath().changeExt(".graph.json");
    }

    /** Reads a successful build record, rejecting older graph dumps that lack packaging inputs. */
    public static BuildGraph load(Project project) throws IOException {
        IResource resource = resource(project);
        if (!resource.exists()) {
            throw new IOException("Build graph is missing; run build before bundle");
        }
        JsonNode data = JSON.readTree(resource.getContent());
        if (data == null || !data.isObject() || !data.has("version") || data.get("version").asInt() != VERSION) {
            throw new IOException("Build graph version is unsupported; run build again");
        }
        return new BuildGraph((ObjectNode) data);
    }

    /** Records the graph and byte identity of compiled outputs, before archive preparation changes their representation. */
    public static BuildGraph capture(Project project, ResourceGraph graph, PackageResolver.Plan plan,
            Collection<String> bootstrap, Collection<String> custom) throws IOException, CompileExceptionError {
        ObjectNode data = JSON.createObjectNode();
        data.put("version", VERSION);
        data.put("bob", EngineVersion.sha1);
        data.put("configuration", JSON.valueToTree(project.getCompilationOptions()));
        data.put("bootstrap", JSON.valueToTree(bootstrap));
        data.put("custom", JSON.valueToTree(custom));
        data.put("sources", JSON.valueToTree(plan.sourceOutputs()));
        data.put("primary_sources", JSON.valueToTree(plan.primarySources()));
        data.put("nodes", JSON.readTree(graph.toJSON()));
        ObjectNode outputs = data.putObject("outputs");
        Set<String> paths = new TreeSet<>(plan.resources().keySet());
        paths.add(PackageBuilder.outputPath(project,
                project.getGameProjectResource().disableMinifyPath().changeExt(".projectc")));
        String certificates = project.getProjectProperties().getStringValue("network", "ssl_certificates", "");
        if (!certificates.isBlank()) {
            paths.add("/" + BundleHelper.SSL_CERTIFICATES_NAME);
        }
        for (String path : paths) {
            BundleHelper.throwIfCanceled(project::isBuildCanceled);
            IResource resource = project.getResource(path).disableMinifyPath().output();
            byte[] content = resource.getContent();
            if (content == null) {
                throw new IOException("Missing compiled resource: " + path);
            }
            ObjectNode output = outputs.putObject(path);
            output.put("hash", DigestUtils.sha256Hex(content));
            output.put("size", content.length);
            Set<Task.OutputFlags> flags = project.getOutputFlags(resource.getAbsPath());
            output.put("flags", JSON.valueToTree(flags == null ? Set.of() : flags));
        }
        return new BuildGraph(data);
    }

    /** Atomically publishes the record only after resource and engine builds have succeeded. */
    public void save(Project project) throws IOException {
        IResource resource = resource(project);
        byte[] content = JSON.writerWithDefaultPrettyPrinter().writeValueAsBytes(data);
        if (resource instanceof DefaultResource) {
            Path destination = Path.of(resource.getAbsPath());
            Files.createDirectories(destination.getParent());
            Path temporary = Files.createTempFile(destination.getParent(), ".graph-", ".json");
            try {
                Files.write(temporary, content);
                BundleHelper.throwIfCanceled(project::isBuildCanceled);
                Files.move(temporary, destination, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } finally {
                Files.deleteIfExists(temporary);
            }
        } else {
            BundleHelper.throwIfCanceled(project::isBuildCanceled);
            resource.setContent(content);
        }
        project.getState().putSignature(resource.getAbsPath(), DigestUtils.sha256(content));
    }

    /** Rejects incompatible compilation settings and files changed since the successful build. */
    public void validate(Project project) throws IOException, CompileExceptionError {
        if (!data.get("bob").asText().equals(EngineVersion.sha1)
                || !data.get("configuration").equals(JSON.valueToTree(project.getCompilationOptions()))) {
            throw new CompileExceptionError("Build configuration differs from the saved graph; run build with these target and compiler settings");
        }
        var outputs = data.get("outputs").getFields();
        while (outputs.hasNext()) {
            BundleHelper.throwIfCanceled(project::isBuildCanceled);
            var output = outputs.next();
            IResource resource = project.getResource(output.getKey()).disableMinifyPath().output();
            byte[] content = resource.exists() ? resource.getContent() : null;
            if (content == null || !DigestUtils.sha256Hex(content).equals(output.getValue().get("hash").asText())) {
                throw new CompileExceptionError("Compiled resource is missing or changed: " + output.getKey() + "; run build again");
            }
        }
    }

    /** Changed bootstrap/custom roots or newly matched sources require establishing the complete graph again. */
    public boolean covers(Collection<String> bootstrap, Collection<String> custom, Collection<String> sources) {
        return new TreeSet<>(bootstrap()).equals(new TreeSet<>(bootstrap))
                && new TreeSet<>(custom()).equals(new TreeSet<>(custom))
                && sources().keySet().containsAll(sources);
    }

    /** Restores dependency edges and loading boundaries without parsing compiled protobufs again. */
    public ResourceGraph graph(Project project) {
        return ResourceGraph.fromJSON(project, data.get("nodes"));
    }

    /** Returns the startup roots used by the successful build. */
    public List<String> bootstrap() {
        return JSON.convertValue(data.get("bootstrap"), new TypeReference<List<String>>() {});
    }

    /** Returns the opaque custom-resource roots used by the successful build. */
    public List<String> custom() {
        return JSON.convertValue(data.get("custom"), new TypeReference<List<String>>() {});
    }

    /** Returns actual runtime outputs for each source, including sources with only build-time outputs. */
    public Map<String, List<String>> sources() {
        return JSON.convertValue(data.get("sources"), new TypeReference<Map<String, List<String>>>() {});
    }

    /** Returns the originating source for generated and ordinary runtime outputs. */
    public Map<String, String> primarySources() {
        return JSON.convertValue(data.get("primary_sources"), new TypeReference<Map<String, String>>() {});
    }

    /** Restores compiler output flags, keyed by absolute compiled path as expected by archive preparation. */
    public Map<String, Set<Task.OutputFlags>> outputFlags(Project project) {
        Map<String, Set<Task.OutputFlags>> result = new LinkedHashMap<>();
        var outputs = data.get("outputs").getFields();
        while (outputs.hasNext()) {
            var output = outputs.next();
            Set<Task.OutputFlags> flags = EnumSet.noneOf(Task.OutputFlags.class);
            for (JsonNode flag : output.getValue().get("flags")) {
                flags.add(Task.OutputFlags.valueOf(flag.asText()));
            }
            result.put(project.getResource(output.getKey()).disableMinifyPath().output().getAbsPath(), Set.copyOf(flags));
        }
        return result;
    }
}
