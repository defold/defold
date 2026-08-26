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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParser;
import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

/**
 * Lightweight glTF inspection used while Bob creates its task graph. It finds
 * external buffers that must participate in the task signature and detects
 * whether morph texture outputs have to be discovered with the model importer.
 */
public final class GltfResourceUtil {
    private static final int GLB_MAGIC = 0x46546c67;
    private static final int GLB_VERSION = 2;
    private static final int GLB_JSON_CHUNK_TYPE = 0x4e4f534a;
    private static final int GLB_JSON_OFFSET = 20;

    private static final JsonFactory JSON_FACTORY = new JsonFactory();
    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper(JSON_FACTORY);

    private GltfResourceUtil() {
    }

    public record Metadata(List<String> externalBufferUris, boolean hasMorphTargets) {
        public Metadata {
            externalBufferUris = List.copyOf(externalBufferUris);
        }
    }

    private record JsonRange(int offset, int length) {
    }

    /**
     * Scans a glTF or GLB resource without loading external buffers or decoding
     * model geometry.
     */
    public static Metadata scan(IResource sceneResource) throws IOException {
        byte[] content = sceneResource.getContent();
        if (content == null) {
            throw new IOException(String.format("glTF resource '%s' has no content", sceneResource.getPath()));
        }
        return scan(content, sceneResource.getPath());
    }

    static Metadata scan(byte[] content, String path) throws IOException {
        String suffix = BuilderUtil.getSuffix(path).toLowerCase(Locale.ROOT);
        JsonRange jsonRange;
        if ("gltf".equals(suffix)) {
            jsonRange = new JsonRange(0, content.length);
        } else if ("glb".equals(suffix)) {
            jsonRange = getGlbJsonRange(content);
        } else {
            throw new IOException(String.format("Unsupported glTF resource extension in '%s'", path));
        }

        try (JsonParser parser = JSON_FACTORY.createJsonParser(content, jsonRange.offset(), jsonRange.length())) {
            JsonNode root = OBJECT_MAPPER.readTree(parser);
            if (root == null || !root.isObject()) {
                throw new IOException("glTF root must be an object");
            }
            return scanJson(root);
        } catch (IOException e) {
            throw new IOException(String.format("Failed to inspect glTF resource '%s': %s", path, e.getMessage()), e);
        }
    }

    /**
     * Adds previously scanned external buffer resources as direct task inputs.
     */
    public static void addExternalBufferInputs(IResource sceneResource, Task.TaskBuilder<?> taskBuilder,
                                               Metadata metadata)
            throws CompileExceptionError {
        for (String uri : metadata.externalBufferUris()) {
            IResource resource = resolveExternalBuffer(sceneResource, uri);
            if (resource == null || !resource.exists()) {
                throw new CompileExceptionError(sceneResource, 0,
                        String.format("External glTF buffer '%s' does not exist", uri));
            }
            taskBuilder.addInput(resource);
        }
    }

    /**
     * Resolves a glTF buffer URI relative to its scene resource.
     */
    public static IResource resolveExternalBuffer(IResource sceneResource, String uri) {
        if (uri == null || uri.isEmpty() || isDataUri(uri)) {
            return null;
        }
        return sceneResource.getResource(uri);
    }

    /**
     * Shared model-importer resolver for external glTF buffers.
     */
    public static final class ResourceDataResolver implements ModelImporterJni.DataResolver {
        private final IResource sceneResource;

        public ResourceDataResolver(IResource sceneResource) {
            this.sceneResource = sceneResource;
        }

        @Override
        public byte[] getData(String path, String uri) {
            IResource resource = resolveExternalBuffer(sceneResource, uri);
            if (resource == null) {
                return null;
            }
            try {
                return resource.getContent();
            } catch (IOException e) {
                return null;
            }
        }
    }

    private static Metadata scanJson(JsonNode root) {
        LinkedHashSet<String> externalBufferUris = new LinkedHashSet<>();
        JsonNode buffers = root.path("buffers");
        if (buffers.isArray()) {
            for (JsonNode buffer : buffers) {
                JsonNode uriNode = buffer.get("uri");
                if (uriNode != null && uriNode.isTextual()) {
                    String uri = uriNode.getTextValue();
                    if (!uri.isEmpty() && !isDataUri(uri)) {
                        externalBufferUris.add(uri);
                    }
                }
            }
        }

        boolean hasMorphTargets = false;
        JsonNode meshes = root.path("meshes");
        if (meshes.isArray()) {
            for (JsonNode mesh : meshes) {
                JsonNode primitives = mesh.path("primitives");
                if (!primitives.isArray()) {
                    continue;
                }
                for (JsonNode primitive : primitives) {
                    JsonNode targets = primitive.path("targets");
                    if (targets.isArray() && targets.size() > 0) {
                        hasMorphTargets = true;
                        break;
                    }
                }
                if (hasMorphTargets) {
                    break;
                }
            }
        }

        return new Metadata(new ArrayList<>(externalBufferUris), hasMorphTargets);
    }

    private static JsonRange getGlbJsonRange(byte[] content) throws IOException {
        if (content.length < GLB_JSON_OFFSET) {
            throw new IOException("GLB data is too short");
        }

        ByteBuffer buffer = ByteBuffer.wrap(content).order(ByteOrder.LITTLE_ENDIAN);
        if (buffer.getInt(0) != GLB_MAGIC) {
            throw new IOException("Invalid GLB magic");
        }
        if (buffer.getInt(4) != GLB_VERSION) {
            throw new IOException("Unsupported GLB version");
        }

        long totalLength = Integer.toUnsignedLong(buffer.getInt(8));
        if (totalLength < GLB_JSON_OFFSET || totalLength > content.length) {
            throw new IOException("Invalid GLB total length");
        }

        long jsonLength = Integer.toUnsignedLong(buffer.getInt(12));
        if (buffer.getInt(16) != GLB_JSON_CHUNK_TYPE) {
            throw new IOException("GLB first chunk is not JSON");
        }
        long jsonEnd = GLB_JSON_OFFSET + jsonLength;
        if (jsonLength > Integer.MAX_VALUE || jsonEnd > totalLength) {
            throw new IOException("Invalid GLB JSON chunk length");
        }
        return new JsonRange(GLB_JSON_OFFSET, (int) jsonLength);
    }

    private static boolean isDataUri(String uri) {
        return uri.regionMatches(true, 0, "data:", 0, 5);
    }
}
