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

package com.dynamo.bob.fs;

import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Collection;

/**
 * A read-only resource synthesized from a glTF asset container.
 */
public abstract class GltfResource extends AbstractResource<IFileSystem> {

    public enum Kind {
        MATERIAL,
        IMAGE,
        MESH
    }

    private final IResource sourceResource;
    private final Kind kind;
    private final int index;
    private final String name;
    private final byte[] content;
    private final long lastModified;

    protected GltfResource(IFileSystem fileSystem, IResource sourceResource, String path, Kind kind, int index, String name, byte[] content) {
        super(fileSystem, path);
        this.sourceResource = sourceResource;
        this.kind = kind;
        this.index = index;
        this.name = name;
        this.content = content.clone();
        this.lastModified = sourceResource.getLastModified();
    }

    public IResource getSourceResource() {
        return sourceResource;
    }

    public Kind getKind() {
        return kind;
    }

    public int getIndex() {
        return index;
    }

    public String getName() {
        return name;
    }

    @Override
    public byte[] getContent() {
        return content.clone();
    }

    @Override
    public void setContent(byte[] content) throws IOException {
        throw readOnlyException();
    }

    @Override
    public void appendContent(byte[] content) throws IOException {
        throw readOnlyException();
    }

    @Override
    public boolean exists() {
        return true;
    }

    @Override
    public void remove() {
        throw new UnsupportedOperationException("glTF resources are read-only");
    }

    @Override
    public void setContent(InputStream stream) throws IOException {
        throw readOnlyException();
    }

    @Override
    public long getLastModified() {
        return lastModified;
    }

    @Override
    public boolean isFile() {
        return true;
    }

    private IOException readOnlyException() {
        return new IOException("glTF resources are read-only");
    }

    private static void usage() {
        System.err.println("Usage: GltfResource <asset.gltf> [--resource <virtual-resource-path>]");
    }

    private static String toHex(byte[] bytes) {
        StringBuilder builder = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            builder.append(String.format("%02x", value & 0xff));
        }
        return builder.toString();
    }

    private static void printResource(GltfResource resource, PrintStream output) throws IOException {
        output.printf("%s[%d]%n", resource.getKind().name().toLowerCase(), resource.getIndex());
        output.printf("  path: %s%n", resource.getPath());
        output.printf("  name: %s%n", resource.getName());
        output.printf("  byte_length: %d%n", resource.getContent().length);
        output.printf("  sha1: %s%n", toHex(resource.sha1()));

        if (resource instanceof GltfMaterialResource) {
            GltfMaterialResource material = (GltfMaterialResource)resource;
            output.println("  content:");
            String content = new String(resource.getContent(), StandardCharsets.UTF_8);
            for (String line : content.split("\\R")) {
                output.printf("    %s%n", line);
            }
        } else if (resource instanceof GltfImageResource) {
            GltfImageResource image = (GltfImageResource)resource;
            output.printf("  uri: %s%n", debugUri(image.getUri()));
            output.printf("  source: %s%n", image.getSourceKind());
            output.printf("  mime_type: %s%n", image.getMimeType());
            if (image.getWidth() >= 0 && image.getHeight() >= 0) {
                output.printf("  dimensions: %dx%d%n", image.getWidth(), image.getHeight());
            }
            for (GltfContainer.TextureMetadata texture : image.getTextures()) {
                output.printf("  texture[%d]: name=%s sampler=%d min=%d mag=%d wrap_s=%d wrap_t=%d basisu=%s%n",
                        texture.getIndex(), texture.getName(), texture.getSamplerIndex(), texture.getMinFilter(),
                        texture.getMagFilter(), texture.getWrapS(), texture.getWrapT(), texture.isBasisu());
            }
        } else if (resource instanceof GltfMeshResource) {
            GltfMeshResource mesh = (GltfMeshResource)resource;
            output.printf("  name_generated: %s%n", mesh.isNameGenerated());
            output.printf("  primitive_count: %d%n", mesh.getPrimitiveCount());
            output.printf("  vertex_count: %d%n", mesh.getVertexCount());
        }
    }

    private static String debugUri(String uri) {
        if (uri == null || !uri.regionMatches(true, 0, "data:", 0, 5)) {
            return uri;
        }

        int comma = uri.indexOf(',');
        if (comma < 0) {
            return "data:<redacted>";
        }
        return String.format("data:<redacted>,<%d encoded characters>", uri.length() - comma - 1);
    }

    /**
     * Debug entry point for inspecting the virtual resources produced by a glTF
     * container. Image bytes are never written to stdout; only metadata is shown.
     */
    public static void main(String[] args) {
        int status = run(args);
        if (status != 0) {
            System.exit(status);
        }
    }

    private static int run(String[] args) {
        System.setProperty("java.awt.headless", "true");

        if (args.length != 1 && (args.length != 3 || !"--resource".equals(args[1]))) {
            usage();
            return 2;
        }

        Path inputPath = Paths.get(args[0]).toAbsolutePath().normalize();
        if (!Files.isRegularFile(inputPath)) {
            System.err.printf("glTF file does not exist: %s%n", inputPath);
            return 1;
        }

        DefaultFileSystem fileSystem = new DefaultFileSystem();
        try {
            Path rootPath = inputPath.getParent();
            fileSystem.setRootDirectory(rootPath.toString());
            fileSystem.setBuildDirectory("build/default");

            IResource sourceResource = fileSystem.get(inputPath.getFileName().toString());
            GltfContainer container = GltfContainer.load(fileSystem, sourceResource);

            System.out.printf("container: %s%n", inputPath);
            System.out.flush();
            for (String diagnostic : container.getDiagnostics()) {
                System.err.printf("warning: %s%n", diagnostic);
            }

            if (args.length == 3) {
                GltfResource resource = container.getResource(args[2]);
                if (resource == null) {
                    System.err.printf("Virtual resource does not exist: %s%n", args[2]);
                    return 1;
                }
                printResource(resource, System.out);
            } else {
                Collection<GltfResource> resources = container.getResources();
                for (GltfResource resource : resources) {
                    printResource(resource, System.out);
                }
            }
            return container.getDiagnostics().isEmpty() ? 0 : 1;
        } catch (Exception e) {
            System.err.printf("Failed to inspect '%s': %s%n", inputPath, e.getMessage());
            return 1;
        } finally {
            fileSystem.close();
        }
    }
}
